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
@end

@implementation BatteryMonitorApp

- (instancetype)init {
    self = [super init];
    if (self) {
        statusItem_ = nil;
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
        delete razerDevice_;
        razerDevice_ = nil;
    }
    [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    // Initialize status bar item
    NSStatusBar* statusBar = [NSStatusBar systemStatusBar];
    statusItem_ = [statusBar statusItemWithLength:NSVariableStatusItemLength];
    [statusItem_ setHighlightMode:YES];
    
    // Create menu
    NSMenu* menu = [[NSMenu alloc] init];
    NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit" 
                                                       action:@selector(terminate:) 
                                                keyEquivalent:@"q"];
    [quitItem setTarget:NSApp];
    [menu addItem:quitItem];
    [statusItem_ setMenu:menu];
    
    // Set initial display
    [statusItem_ setTitle:@"Razer: --%"];
    
    // Connect to device
    if (!razerDevice_->connect()) {
        [statusItem_ setTitle:@"Razer: Not Found"];
        NSLog(@"Failed to connect to Razer Viper V2 Pro");
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
    if (!razerDevice_->isConnected()) {
        [statusItem_ setTitle:@"Razer: Disconnected"];
        // Try to reconnect
        if (razerDevice_->connect()) {
            NSLog(@"Reconnected to Razer device");
        }
        return;
    }
    
    uint8_t batteryPercent = 0;
    if (razerDevice_->queryBattery(batteryPercent)) {
        // Only update if we got valid data (not 0% from fallback)
        if (batteryPercent > 0 || lastBatteryLevel_ == 0) {
            lastBatteryLevel_ = batteryPercent;
            NSString* title = [NSString stringWithFormat:@"Razer: %d%%", batteryPercent];
            [statusItem_ setTitle:title];
            
            // Show notification if battery < 20% (only once per low battery event)
            if (batteryPercent < 20 && batteryPercent > 0 && !notificationShown_) {
                [self showLowBatteryNotification:batteryPercent];
                notificationShown_ = true;
            } else if (batteryPercent >= 20) {
                notificationShown_ = false;
            }
        } else {
            // Keep last known value if query failed
            NSString* title = [NSString stringWithFormat:@"Razer: %d%%", lastBatteryLevel_];
            [statusItem_ setTitle:title];
        }
    } else {
        // Query failed - show error but keep last known value if available
        if (lastBatteryLevel_ > 0) {
            NSString* title = [NSString stringWithFormat:@"Razer: %d%% (?)", lastBatteryLevel_];
            [statusItem_ setTitle:title];
        } else {
            [statusItem_ setTitle:@"Razer: Error"];
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
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        BatteryMonitorApp* delegate = [[BatteryMonitorApp alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}

