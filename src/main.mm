#import <Cocoa/Cocoa.h>
#import "RazerDevice.hpp"

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
@end

@implementation BatteryMonitorApp

- (instancetype)init {
    self = [super init];
    if (self) {
        statusItem_ = nil;
        razerDevice_ = nil;  // Don't create yet - wait for UI
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
        delete razerDevice_;
        razerDevice_ = nil;
    }
    [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    // STEP 1: Create UI FIRST (before any device operations)
    NSStatusBar* statusBar = [NSStatusBar systemStatusBar];
    statusItem_ = [[statusBar statusItemWithLength:NSVariableStatusItemLength] retain];
    
    // Use modern API for button title
    statusItem_.button.title = @"🖱️ ...";
    
    // Create menu
    NSMenu* menu = [[NSMenu alloc] init];
    NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit" 
                                                       action:@selector(terminate:) 
                                                keyEquivalent:@"q"];
    [quitItem setTarget:NSApp];
    [menu addItem:quitItem];
    statusItem_.menu = menu;
    
    // STEP 2: Force UI to appear immediately
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
    
    // STEP 3: Connect to device after UI is visible (delayed)
    [self performSelector:@selector(connectToDevice) withObject:nil afterDelay:0.5];
}

- (void)connectToDevice {
    // Create device instance
    razerDevice_ = new RazerDevice();
    
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
    
    // Set up polling timer (15 minutes = 900 seconds)
    pollTimer_ = [NSTimer scheduledTimerWithTimeInterval:900.0
                                                   target:self
                                                 selector:@selector(pollBattery:)
                                                 userInfo:nil
                                                  repeats:YES];
}

- (void)updateBatteryDisplay {
    if (razerDevice_ == nil) {
        statusItem_.button.title = @"🖱️ ...";
        return;
    }
    
    if (!razerDevice_->isConnected()) {
        statusItem_.button.title = @"🖱️ Disconnected";
        // Try to reconnect
        if (razerDevice_->connect()) {
            NSLog(@"Reconnected to Razer device");
        }
        return;
    }
    
    uint8_t batteryPercent = 0;
    if (razerDevice_->queryBattery(batteryPercent)) {
        lastBatteryLevel_ = batteryPercent;
        statusItem_.button.title = [NSString stringWithFormat:@"🖱️ %d%%", batteryPercent];
        
        // Show notification if battery < 20% (only once per low battery event)
        if (batteryPercent < 20 && batteryPercent > 0 && !notificationShown_) {
            [self showLowBatteryNotification:batteryPercent];
            notificationShown_ = true;
        } else if (batteryPercent >= 20) {
            notificationShown_ = false;
        }
    } else {
        // Query failed - show error but keep last known value if available
        if (lastBatteryLevel_ > 0) {
            statusItem_.button.title = [NSString stringWithFormat:@"🖱️ %d%% (?)", lastBatteryLevel_];
        } else {
            statusItem_.button.title = @"🖱️ Error";
        }
    }
}

- (void)pollBattery:(NSTimer*)timer {
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
    (void)argc;  // Unused
    (void)argv;  // Unused
    
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        
        // Force Menu Bar Accessory mode (required when running raw binary without .app bundle)
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        
        BatteryMonitorApp* delegate = [[BatteryMonitorApp alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}

