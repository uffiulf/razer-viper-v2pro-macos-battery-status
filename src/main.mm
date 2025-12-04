#import <Cocoa/Cocoa.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/usb/IOUSBLib.h>
#import "RazerDevice.hpp"

// USB Vendor/Product IDs for Razer Viper V2 Pro
#define RAZER_VENDOR_ID  0x1532
#define RAZER_PRODUCT_ID 0x00A6

// Forward declaration for C callback
@class BatteryMonitorApp;
static BatteryMonitorApp* gAppInstance = nil;

// C callback for USB events
void usbDeviceCallback(void* refcon, io_iterator_t iterator) {
    (void)refcon;
    
    // Drain the iterator
    io_service_t device;
    while ((device = IOIteratorNext(iterator))) {
        IOObjectRelease(device);
    }
    
    // Trigger update on main thread
    if (gAppInstance) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [gAppInstance handleUSBEvent];
        });
    }
}

@interface BatteryMonitorApp : NSObject <NSApplicationDelegate> {
    NSStatusItem* statusItem_;
    RazerDevice* razerDevice_;
    NSTimer* pollTimer_;
    uint8_t lastBatteryLevel_;
    bool notificationShown_;
    
    // IOKit USB notification
    IONotificationPortRef notificationPort_;
    io_iterator_t deviceAddedIterator_;
    io_iterator_t deviceRemovedIterator_;
}

- (void)updateBatteryDisplay;
- (void)pollBattery:(NSTimer*)timer;
- (void)connectToDevice;
- (void)handleUSBEvent;
- (void)setupUSBNotifications;
@end

@implementation BatteryMonitorApp

- (instancetype)init {
    self = [super init];
    if (self) {
        statusItem_ = nil;
        razerDevice_ = nil;
        pollTimer_ = nil;
        lastBatteryLevel_ = 0;
        notificationShown_ = false;
        notificationPort_ = NULL;
        deviceAddedIterator_ = 0;
        deviceRemovedIterator_ = 0;
        gAppInstance = self;
    }
    return self;
}

- (void)dealloc {
    gAppInstance = nil;
    
    // Clean up USB notifications
    if (deviceAddedIterator_) {
        IOObjectRelease(deviceAddedIterator_);
    }
    if (deviceRemovedIterator_) {
        IOObjectRelease(deviceRemovedIterator_);
    }
    if (notificationPort_) {
        IONotificationPortDestroy(notificationPort_);
    }
    
    if (pollTimer_) {
        [pollTimer_ invalidate];
        pollTimer_ = nil;
    }
    if (razerDevice_) {
        delete razerDevice_;
        razerDevice_ = nil;
    }
    [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    // STEP 1: Create UI FIRST
    NSStatusBar* statusBar = [NSStatusBar systemStatusBar];
    statusItem_ = [[statusBar statusItemWithLength:NSVariableStatusItemLength] retain];
    
    statusItem_.button.title = @"🖱️ ...";
    statusItem_.button.toolTip = @"Razer Viper V2 Pro";
    
    // Create menu
    NSMenu* menu = [[NSMenu alloc] init];
    
    NSMenuItem* refreshItem = [[NSMenuItem alloc] initWithTitle:@"Refresh" 
                                                         action:@selector(manualRefresh:) 
                                                  keyEquivalent:@"r"];
    [refreshItem setTarget:self];
    [menu addItem:refreshItem];
    
    [menu addItem:[NSMenuItem separatorItem]];
    
    NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit" 
                                                       action:@selector(terminate:) 
                                                keyEquivalent:@"q"];
    [quitItem setTarget:NSApp];
    [menu addItem:quitItem];
    statusItem_.menu = menu;
    
    // STEP 2: Force UI to appear immediately
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
    
    // STEP 3: Setup USB hotplug notifications
    [self setupUSBNotifications];
    
    // STEP 4: Connect to device
    [self performSelector:@selector(connectToDevice) withObject:nil afterDelay:0.5];
}

- (void)setupUSBNotifications {
    // Create notification port
    notificationPort_ = IONotificationPortCreate(kIOMainPortDefault);
    if (!notificationPort_) {
        NSLog(@"Failed to create IONotificationPort");
        return;
    }
    
    // Add to run loop
    CFRunLoopSourceRef runLoopSource = IONotificationPortGetRunLoopSource(notificationPort_);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode);
    
    // Create matching dictionary for Razer devices
    CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
    if (!matchingDict) {
        NSLog(@"Failed to create matching dictionary");
        return;
    }
    
    // Add VID/PID
    int vid = RAZER_VENDOR_ID;
    int pid = RAZER_PRODUCT_ID;
    CFNumberRef vidRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vid);
    CFNumberRef pidRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid);
    CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorID), vidRef);
    CFDictionarySetValue(matchingDict, CFSTR(kUSBProductID), pidRef);
    CFRelease(vidRef);
    CFRelease(pidRef);
    
    // Register for device added (need to retain dict for second use)
    CFRetain(matchingDict);
    
    kern_return_t kr = IOServiceAddMatchingNotification(
        notificationPort_,
        kIOFirstMatchNotification,
        matchingDict,
        usbDeviceCallback,
        (__bridge void*)self,
        &deviceAddedIterator_
    );
    
    if (kr != KERN_SUCCESS) {
        NSLog(@"Failed to register for device added: 0x%x", kr);
    } else {
        // Drain initial iterator
        io_service_t device;
        while ((device = IOIteratorNext(deviceAddedIterator_))) {
            IOObjectRelease(device);
        }
        NSLog(@"USB hotplug: Listening for device connections");
    }
    
    // Register for device removed
    kr = IOServiceAddMatchingNotification(
        notificationPort_,
        kIOTerminatedNotification,
        matchingDict,
        usbDeviceCallback,
        (__bridge void*)self,
        &deviceRemovedIterator_
    );
    
    if (kr != KERN_SUCCESS) {
        NSLog(@"Failed to register for device removed: 0x%x", kr);
    } else {
        // Drain initial iterator
        io_service_t device;
        while ((device = IOIteratorNext(deviceRemovedIterator_))) {
            IOObjectRelease(device);
        }
        NSLog(@"USB hotplug: Listening for device disconnections");
    }
}

- (void)handleUSBEvent {
    NSLog(@"USB event detected - refreshing...");
    
    // Reconnect to device (may have changed mode)
    if (razerDevice_) {
        razerDevice_->disconnect();
        
        // Small delay to let USB settle
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (razerDevice_->connect()) {
                [self updateBatteryDisplay];
            } else {
                statusItem_.button.title = @"🖱️ Not Found";
            }
        });
    }
}

- (void)manualRefresh:(id)sender {
    (void)sender;
    [self handleUSBEvent];
}

- (void)connectToDevice {
    // Create device instance if needed
    if (!razerDevice_) {
        razerDevice_ = new RazerDevice();
    }
    
    // Try to connect
    if (!razerDevice_->connect()) {
        statusItem_.button.title = @"🖱️ Not Found";
        NSLog(@"Failed to connect to Razer Viper V2 Pro");
        
        // Retry in 10 seconds
        [self performSelector:@selector(connectToDevice) withObject:nil afterDelay:10.0];
        return;
    }
    
    // Initial battery query
    [self updateBatteryDisplay];
    
    // Set up polling timer (30 seconds for responsive updates)
    // USB hotplug doesn't work for this because the wireless receiver stays connected
    // when the mouse cable is plugged in - only the mouse changes mode internally
    if (!pollTimer_) {
        pollTimer_ = [NSTimer scheduledTimerWithTimeInterval:30.0  // 30 seconds
                                                       target:self
                                                     selector:@selector(pollBattery:)
                                                     userInfo:nil
                                                      repeats:YES];
    }
}

- (void)updateBatteryDisplay {
    if (razerDevice_ == nil) {
        statusItem_.button.title = @"🖱️ ...";
        return;
    }
    
    if (!razerDevice_->isConnected()) {
        statusItem_.button.title = @"🖱️ Disconnected";
        if (razerDevice_->connect()) {
            NSLog(@"Reconnected to Razer device");
        }
        return;
    }
    
    uint8_t batteryPercent = 0;
    if (razerDevice_->queryBattery(batteryPercent)) {
        lastBatteryLevel_ = batteryPercent;
        
        // Check charging status
        bool isCharging = false;
        razerDevice_->queryChargingStatus(isCharging);
        
        // Format title with charging indicator
        NSString* titleText;
        if (isCharging) {
            titleText = [NSString stringWithFormat:@"🖱️ %d%% ⚡", batteryPercent];
        } else {
            titleText = [NSString stringWithFormat:@"🖱️ %d%%", batteryPercent];
        }
        
        // Color based on battery level
        NSColor* textColor;
        if (batteryPercent <= 20) {
            textColor = [NSColor systemRedColor];
        } else if (batteryPercent <= 30) {
            textColor = [NSColor systemOrangeColor];
        } else {
            textColor = [NSColor labelColor];
        }
        
        // Apply colored title
        NSDictionary* attrs = @{
            NSForegroundColorAttributeName: textColor,
            NSFontAttributeName: [NSFont menuBarFontOfSize:0]
        };
        statusItem_.button.attributedTitle = [[NSAttributedString alloc] initWithString:titleText attributes:attrs];
        
        // Low battery notification
        if (batteryPercent < 20 && batteryPercent > 0 && !notificationShown_ && !isCharging) {
            [self showLowBatteryNotification:batteryPercent];
            notificationShown_ = true;
        } else if (batteryPercent >= 20 || isCharging) {
            notificationShown_ = false;
        }
    } else {
        NSString* errorText;
        if (lastBatteryLevel_ > 0) {
            errorText = [NSString stringWithFormat:@"🖱️ %d%% (?)", lastBatteryLevel_];
        } else {
            errorText = @"🖱️ Error";
        }
        NSDictionary* attrs = @{
            NSForegroundColorAttributeName: [NSColor systemGrayColor],
            NSFontAttributeName: [NSFont menuBarFontOfSize:0]
        };
        statusItem_.button.attributedTitle = [[NSAttributedString alloc] initWithString:errorText attributes:attrs];
    }
}

- (void)pollBattery:(NSTimer*)timer {
    (void)timer;
    [self updateBatteryDisplay];
}

- (void)showLowBatteryNotification:(uint8_t)batteryPercent {
    NSUserNotification* notification = [[NSUserNotification alloc] init];
    [notification setTitle:@"Razer Viper V2 Pro - Low Battery"];
    [notification setInformativeText:[NSString stringWithFormat:@"Battery level is %d%%. Please charge your mouse.", batteryPercent]];
    [notification setSoundName:NSUserNotificationDefaultSoundName];
    
    NSUserNotificationCenter* center = [NSUserNotificationCenter defaultUserNotificationCenter];
    [center deliverNotification:notification];
    
    [notification release];
}

- (void)applicationWillTerminate:(NSNotification*)notification {
    (void)notification;
    if (pollTimer_) {
        [pollTimer_ invalidate];
        pollTimer_ = nil;
    }
    if (razerDevice_) {
        razerDevice_->disconnect();
    }
}

@end

int main(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;
    
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        
        BatteryMonitorApp* delegate = [[BatteryMonitorApp alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
