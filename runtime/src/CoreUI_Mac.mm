#include <Cocoa/Cocoa.h>
#include <QuartzCore/QuartzCore.h>

extern "C" {

    void* create_native_window(int width, int height, const char* title) {
        @autoreleasepool {
            // 1. Create Application (if not already existing)
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            [NSApp finishLaunching];

            // 2. Create Window
            NSRect frame = NSMakeRect(0, 0, width, height);
            NSWindowStyleMask style = NSWindowStyleMaskTitled | 
                                      NSWindowStyleMaskClosable | 
                                      NSWindowStyleMaskResizable | 
                                      NSWindowStyleMaskMiniaturizable;

            NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                           styleMask:style
                                                             backing:NSBackingStoreBuffered
                                                               defer:NO];
            
            [window setTitle:[NSString stringWithUTF8String:title]];
            [window makeKeyAndOrderFront:nil];
            [NSApp activateIgnoringOtherApps:YES];

            // 3. Create View
            NSView* view = [[NSView alloc] initWithFrame:frame];
            [window setContentView:view];

            // 4. Create Metal Layer
            CAMetalLayer* layer = [CAMetalLayer layer];
            [view setLayer:layer];
            [view setWantsLayer:YES];

            // Retain the layer so it doesn't get deallocated immediately? 
            // In ARC context (if enabled), relying on view ownership. 
            // Since we return void*, we should probably retain it if we plan to pass ownership or just ensure view stays alive.
            // For this simple shim, the window/view ownership graph keeps it alive.
            
            return (__bridge void*)layer;
        }
    }

    void process_events() {
        @autoreleasepool {
             NSEvent* event;
             while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny 
                                                untilDate:[NSDate distantPast] 
                                                   inMode:NSDefaultRunLoopMode 
                                                  dequeue:YES])) {
                 [NSApp sendEvent:event];
                 [NSApp updateWindows];
             }
        }
    }
}
