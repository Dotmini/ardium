// Topic: GUI (Cocoa)
// Create a Window with a Button

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 200, 200)
        styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    [window setTitle:@"Obj-C GUI"];
    
    NSButton *btn = [NSButton buttonWithTitle:@"Click Me!" target:nil action:nil];
    [window.contentView addSubview:btn];
    
    [window makeKeyAndOrderFront:nil];
}
@end

int main(int argc, const char * argv[]) {
    return NSApplicationMain(argc, argv);
}
