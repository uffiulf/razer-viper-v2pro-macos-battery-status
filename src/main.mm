#import <Cocoa/Cocoa.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/usb/IOUSBLib.h>
#import "RazerDevice.hpp"

// Forward declaration
@class BatteryMonitorApp;

// Static callback for RazerDevice monitoring
static void onDeviceChange(void* context) {
    BatteryMonitorApp* app = (__bridge BatteryMonitorApp*)context;
    // Ensure we run on main thread for UI updates
    dispatch_async(dispatch_get_main_queue(), ^{
        [app performSelector:@selector(handleUSBEvent)];
    });
}

@interface BatteryMonitorApp : NSObject <NSApplicationDelegate> {
    NSStatusItem* statusItem_;
    RazerDevice* razerDevice_;
    NSTimer* pollTimer_;
    uint8_t lastBatteryLevel_;
    bool notificationShown_;
}

- (void)updateBatteryDisplay;
- (void)pollBattery:(NSTimer*)timer;
- (void)connectToDevice;
- (void)handleUSBEvent;
- (NSImage*)mouseIconWithColor:(NSColor*)color;
@end

@implementation BatteryMonitorApp

- (instancetype)init {
    self = [super init];
    if (self) {
        statusItem_ = nil;
        // Create device instance immediately and keep it alive
        razerDevice_ = new RazerDevice();
        pollTimer_ = nil;
        lastBatteryLevel_ = 0;
        notificationShown_ = false;
    }
    return self;
}

- (void)dealloc {
    if (pollTimer_) {
        [pollTimer_ invalidate];
        pollTimer_ = nil;
    }
    if (razerDevice_) {
        // Stop monitoring before deleting
        razerDevice_->stopMonitoring();
        delete razerDevice_;
        razerDevice_ = nil;
    }
    [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    // STEP 1: Create UI FIRST
    NSStatusBar* statusBar = [NSStatusBar systemStatusBar];
    statusItem_ = [[statusBar statusItemWithLength:NSVariableStatusItemLength] retain];
    
    NSImage* mouseIcon = [self mouseIconWithColor:[NSColor whiteColor]];
    if (mouseIcon) {
        statusItem_.button.image = mouseIcon;
        statusItem_.button.title = @"...";
        statusItem_.button.imagePosition = NSImageLeft;
    } else {
        statusItem_.button.title = @"🖱️ ...";
    }
    statusItem_.button.toolTip = @"Razer Battery Monitor";
    
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
    
    // STEP 3: Start IOKit Hotplug Monitoring via RazerDevice
    if (razerDevice_) {
        razerDevice_->startMonitoring(onDeviceChange, (__bridge void*)self);
    }
    
    // STEP 4: Connect to device
    [self performSelector:@selector(connectToDevice) withObject:nil afterDelay:0.5];
}

- (void)handleUSBEvent {
    NSLog(@"USB event detected - refreshing...");
    
    // Reconnect to device (may have changed mode)
    if (razerDevice_) {
        razerDevice_->disconnect();
        
        // Aggressive Retry Logic: Try to reconnect multiple times
        // This ensures we catch the device as soon as it finishes enumeration
        
        // 1s
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (razerDevice_->connect()) [self updateBatteryDisplay];
        });

        // 3s
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(3.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (razerDevice_->connect()) [self updateBatteryDisplay];
        });

        // 6s
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(6.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (razerDevice_->connect()) [self updateBatteryDisplay];
        });
        
        // 10s
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(10.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (razerDevice_->connect()) [self updateBatteryDisplay];
        });
        
        // 15s - Last attempt
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(15.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
             if (razerDevice_->connect()) {
                 [self updateBatteryDisplay];
             } else {
                // Only show "Not Found" if ALL attempts fail after 15 seconds
                NSImage* icon = [self mouseIconWithColor:[NSColor systemGrayColor]];
                if (icon) {
                    statusItem_.button.image = icon;
                    statusItem_.button.title = @"Not Found";
                } else {
                    statusItem_.button.image = nil;
                    statusItem_.button.title = @"🖱️ Not Found";
                }
             }
        });
    }
}

- (void)manualRefresh:(id)sender {
    (void)sender;
    [self handleUSBEvent];
}

- (void)connectToDevice {
    // Try to connect
    if (!razerDevice_->connect()) {
        NSImage* icon = [self mouseIconWithColor:[NSColor systemGrayColor]];
        if (icon) {
            statusItem_.button.image = icon;
            statusItem_.button.title = @"Not Found";
        } else {
            statusItem_.button.image = nil;
            statusItem_.button.title = @"🖱️ Not Found";
        }
        NSLog(@"Failed to connect to Razer Viper V2 Pro");
        
        // Retry in 10 seconds if initial connection fails
        [self performSelector:@selector(connectToDevice) withObject:nil afterDelay:10.0];
        return;
    }
    
    // Initial battery query
    [self updateBatteryDisplay];
    
    // Set up polling timer (30 seconds)
    // We still keep this as a fallback for battery % changes over time
    if (!pollTimer_) {
        pollTimer_ = [NSTimer scheduledTimerWithTimeInterval:30.0
                                                       target:self
                                                     selector:@selector(pollBattery:)
                                                     userInfo:nil
                                                      repeats:YES];
    }
}

- (void)updateBatteryDisplay {
    if (razerDevice_ == nil) {
        NSImage* icon = [self mouseIconWithColor:[NSColor whiteColor]];
        if (icon) {
            statusItem_.button.image = icon;
            statusItem_.button.title = @"...";
        } else {
            statusItem_.button.image = nil;
            statusItem_.button.title = @"🖱️ ...";
        }
        return;
    }
    
    if (!razerDevice_->isConnected()) {
        // Only show disconnected if we really can't connect after a retry
        if (!razerDevice_->connect()) {
             NSImage* icon = [self mouseIconWithColor:[NSColor systemGrayColor]];
             if (icon) {
                 statusItem_.button.image = icon;
                 statusItem_.button.title = @"Disconnected";
             } else {
                 statusItem_.button.image = nil;
                 statusItem_.button.title = @"🖱️ Disconnected";
             }
             return;
        }
        NSLog(@"Reconnected to Razer device");
    }
    
    uint8_t batteryPercent = 0;
    if (razerDevice_->queryBattery(batteryPercent)) {
        lastBatteryLevel_ = batteryPercent;
        
        // Check charging status
        bool isCharging = false;
        razerDevice_->queryChargingStatus(isCharging);
        
        // Format title text (battery percentage + charging indicator)
        NSString* titleText;
        NSString* titleTextWithEmoji;
        if (isCharging) {
            titleText = [NSString stringWithFormat:@"%d%% ⚡", batteryPercent];
            titleTextWithEmoji = [NSString stringWithFormat:@"🖱️ %d%% ⚡", batteryPercent];
        } else {
            titleText = [NSString stringWithFormat:@"%d%%", batteryPercent];
            titleTextWithEmoji = [NSString stringWithFormat:@"🖱️ %d%%", batteryPercent];
        }
        
        // Color based on battery level (for both icon and text)
        NSColor* displayColor;
        if (batteryPercent <= 20) {
            displayColor = [NSColor systemRedColor];      // Critical: Red (0-20%)
        } else if (batteryPercent <= 40) {
            displayColor = [NSColor systemYellowColor];   // Warning: Yellow (21-40%)
        } else {
            displayColor = [NSColor systemGreenColor];    // Good: Green (41-100%)
        }
        
        // Set icon (SF Symbol or emoji fallback)
        NSImage* icon = [self mouseIconWithColor:displayColor];
        if (icon) {
            statusItem_.button.image = icon;
            statusItem_.button.title = titleText;
        } else {
            statusItem_.button.image = nil;
            statusItem_.button.title = titleTextWithEmoji;
        }
        
        // Apply colored text
        NSString* finalTitle = icon ? titleText : titleTextWithEmoji;
        NSDictionary* attrs = @{
            NSForegroundColorAttributeName: displayColor,
            NSFontAttributeName: [NSFont menuBarFontOfSize:0]
        };
        statusItem_.button.attributedTitle = [[NSAttributedString alloc] initWithString:finalTitle attributes:attrs];
        
        // Low battery notification
        if (batteryPercent < 20 && batteryPercent > 0 && !notificationShown_ && !isCharging) {
            [self showLowBatteryNotification:batteryPercent];
            notificationShown_ = true;
        } else if (batteryPercent >= 20 || isCharging) {
            notificationShown_ = false;
        }
    } else {
        // If query fails, show cached value with (?) indicator to avoid flickering
        NSString* errorText;
        NSString* errorTextWithEmoji;
        NSColor* errorColor = [NSColor systemGrayColor];
        if (lastBatteryLevel_ > 0) {
            errorText = [NSString stringWithFormat:@"%d%% (?)", lastBatteryLevel_];
            errorTextWithEmoji = [NSString stringWithFormat:@"🖱️ %d%% (?)", lastBatteryLevel_];
        } else {
            errorText = @"Error";
            errorTextWithEmoji = @"🖱️ Error";
        }
        
        NSImage* icon = [self mouseIconWithColor:errorColor];
        if (icon) {
            statusItem_.button.image = icon;
            statusItem_.button.title = errorText;
        } else {
            statusItem_.button.image = nil;
            statusItem_.button.title = errorTextWithEmoji;
        }
        
        NSString* finalTitle = icon ? errorText : errorTextWithEmoji;
        NSDictionary* attrs = @{
            NSForegroundColorAttributeName: errorColor,
            NSFontAttributeName: [NSFont menuBarFontOfSize:0]
        };
        statusItem_.button.attributedTitle = [[NSAttributedString alloc] initWithString:finalTitle attributes:attrs];
    }
}

- (void)pollBattery:(NSTimer*)timer {
    (void)timer;
    [self updateBatteryDisplay];
}

- (NSImage*)mouseIconWithColor:(NSColor*)color {
    (void)color;  // Color parameter reserved for future use
    
    // Try SF Symbol first (macOS 11+)
    if (@available(macOS 11.0, *)) {
        NSImage* icon = [NSImage imageWithSystemSymbolName:@"computermouse.fill" 
                               accessibilityDescription:@"Mouse"];
        if (icon) {
            // Return as template so it adapts to menu bar appearance
            [icon setTemplate:YES];
            return icon;
        }
    }
    
    // Fallback: return nil (text-only display will be used)
    return nil;
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
        razerDevice_->stopMonitoring();
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
