#import <Cocoa/Cocoa.h>
#import <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Global State for Layout ---
// These are accessed by Ardium via __agui_get/set intrinsics
long __native_cursor_x = 0;
long __native_cursor_y = 0;
long __native_layout_mode = 0; // 0=Free, 1=VStack, 2=HStack

// --- Accessors for Ardium ---
long __agui_get_cursor_x() { return __native_cursor_x; }
void __agui_set_cursor_x(long v) { __native_cursor_x = v; }

long __agui_get_cursor_y() { return __native_cursor_y; }
void __agui_set_cursor_y(long v) { __native_cursor_y = v; }

long __agui_get_layout_mode() { return __native_layout_mode; }
void __agui_set_layout_mode(long v) { __native_layout_mode = v; }

// --- GUI Implementation ---

static void run_on_main(dispatch_block_t block) {
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}
static void run_on_main_async(dispatch_block_t block) {
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_async(dispatch_get_main_queue(), block);
    }
}

@interface WindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation WindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender {
    [NSApp terminate:nil];
    return YES;
}
@end

@interface CustomView : NSView
@end

CustomView* global_view = nil;
NSWindow* global_window = nil;
WindowDelegate* global_delegate = nil;

// Layout Stack Management
NSMutableArray* view_stack = nil; // Stack of NSStackView*

@implementation CustomView
- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor whiteColor] setFill];
    NSRectFill(dirtyRect);
}
- (BOOL)isFlipped { return YES; }
@end

// --- Layout Primitives ---

static NSStackView* current_stack() {
    if (view_stack && [view_stack count] > 0) {
        return [view_stack lastObject];
    }
    return nil;
}

void __sys_begin_vstack(long spacing, long alignment) {
    run_on_main(^{
        NSStackView* stack = [NSStackView stackViewWithViews:@[]];
        [stack setOrientation:NSUserInterfaceLayoutOrientationVertical];
        
        // Alignment mapping: 0=Leading(Left), 1=Center, 2=Trailing(Right)
        NSLayoutAttribute align = NSLayoutAttributeLeading;
        if (alignment == 1) align = NSLayoutAttributeCenterX;
        if (alignment == 2) align = NSLayoutAttributeTrailing;
        [stack setAlignment:align];
        
        [stack setSpacing:(CGFloat)spacing];
        [stack setTranslatesAutoresizingMaskIntoConstraints:NO];
        // Default edge insets for better look
        [stack setEdgeInsets:NSEdgeInsetsMake(0, 0, 0, 0)];

        NSStackView* parent = current_stack();
        if (parent) {
            [parent addView:stack inGravity:NSStackViewGravityTop];
        } else if (global_view) {
            [global_view addSubview:stack];
            // If root, constraints to fill window
            [NSLayoutConstraint activateConstraints:@[
                [stack.topAnchor constraintEqualToAnchor:global_view.topAnchor constant:10],
                [stack.leadingAnchor constraintEqualToAnchor:global_view.leadingAnchor constant:10],
                [stack.trailingAnchor constraintEqualToAnchor:global_view.trailingAnchor constant:-10],
                [stack.bottomAnchor constraintEqualToAnchor:global_view.bottomAnchor constant:-10]
            ]];
        }
        
        if (!view_stack) view_stack = [[NSMutableArray alloc] init];
        [view_stack addObject:stack];
    });
}

// Compiler Internal Alias
void begin_vstack() { __sys_begin_vstack(8, 0); }

void __sys_begin_hstack(long spacing, long alignment) {
    run_on_main(^{
        NSStackView* stack = [NSStackView stackViewWithViews:@[]];
        [stack setOrientation:NSUserInterfaceLayoutOrientationHorizontal];
        
        // Alignment
        NSLayoutAttribute align = NSLayoutAttributeTop; 
        if (alignment == 1) align = NSLayoutAttributeCenterY;
        if (alignment == 2) align = NSLayoutAttributeBottom;
        [stack setAlignment:align];
        
        [stack setSpacing:(CGFloat)spacing];
        [stack setTranslatesAutoresizingMaskIntoConstraints:NO];
        [stack setEdgeInsets:NSEdgeInsetsMake(0, 0, 0, 0)];

        NSStackView* parent = current_stack();
        if (parent) {
            [parent addView:stack inGravity:NSStackViewGravityTop];
        } else if (global_view) {
            [global_view addSubview:stack];
            // Constraints (fill horizontal, top-aligned)
            [NSLayoutConstraint activateConstraints:@[
                [stack.topAnchor constraintEqualToAnchor:global_view.topAnchor constant:10],
                [stack.leadingAnchor constraintEqualToAnchor:global_view.leadingAnchor constant:10],
                [stack.trailingAnchor constraintEqualToAnchor:global_view.trailingAnchor constant:-10]
            ]];
        }
        
        if (!view_stack) view_stack = [[NSMutableArray alloc] init];
        [view_stack addObject:stack];
    });
}
// Compiler Internal Alias
void begin_hstack() { __sys_begin_hstack(8, 0); }

// Generic End Stack for Compiler


void __sys_set_bg() {
    run_on_main(^{
        // "Midnight Charcoal"
        global_window.backgroundColor = [NSColor colorWithRed:0.1 green:0.1 blue:0.12 alpha:1.0];
        // Make window look modern
        global_window.titlebarAppearsTransparent = YES;
        global_window.styleMask |= NSWindowStyleMaskFullSizeContentView;
    });
}


void __sys_begin_zstack() {
    run_on_main(^{
        // ZStack: NSView that allows overlapping. 
        // We use a regular NSView, but we manage subviews without auto-stacking.
        NSView* zview = [[NSView alloc] init];
        [zview setTranslatesAutoresizingMaskIntoConstraints:NO];
        
        NSStackView* parent = current_stack();
        if (parent) {
            // How to add a non-stack view to stack? Yes, addView works.
            [parent addView:zview inGravity:NSStackViewGravityTop];
            // Constraints for zview size needed? usually it fills space or fits content.
        } else if (global_view) {
            [global_view addSubview:zview];
            [NSLayoutConstraint activateConstraints:@[
                [zview.topAnchor constraintEqualToAnchor:global_view.topAnchor],
                [zview.leadingAnchor constraintEqualToAnchor:global_view.leadingAnchor],
                [zview.trailingAnchor constraintEqualToAnchor:global_view.trailingAnchor],
                [zview.bottomAnchor constraintEqualToAnchor:global_view.bottomAnchor]
            ]];
        }
        
        // We treat it as a "stack" logically, but we need to know its type.
        // For now, push it. But wait, `view_stack` is defined as `NSMutableArray*` (of id).
        // `current_stack()` assumes `NSStackView`.
        // If we push `NSView`, `current_stack` returns it.
        // `[parent addView:stack...]` works if parent is StackView.
        // If parent is `NSView` (ZStack), we must use `addSubview`.
        
        // FIX: We need robust stack management.
        if (!view_stack) view_stack = [[NSMutableArray alloc] init];
        [view_stack addObject:zview];
    });
}

void __sys_end_stack() {
    run_on_main(^{
        if (view_stack && [view_stack count] > 0) {
            [view_stack removeLastObject];
        }
    });
}

void __sys_init_gui() {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

void __sys_create_window(const char* title, long x, long y, long w, long h) {
    run_on_main(^{
        NSRect frame = NSMakeRect(x, y, w, h);
        global_window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskFullSizeContentView)
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        [global_window setTitle:[NSString stringWithUTF8String:title]];
        [global_window setTitlebarAppearsTransparent:YES];
        
        global_delegate = [[WindowDelegate alloc] init];
        [global_window setDelegate:global_delegate];
        
        global_view = [[CustomView alloc] initWithFrame:[[global_window contentView] bounds]];
        [global_window setContentView:global_view];
        
        [global_window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    });
}

void __sys_draw_text(const char* text, long x, long y, long size, long is_bold, long is_centered) {
    run_on_main_async(^{
        NSTextField* label = [NSTextField labelWithString:[NSString stringWithUTF8String:text]];
        if (is_bold) {
            [label setFont:[NSFont boldSystemFontOfSize:size]];
        } else {
            [label setFont:[NSFont systemFontOfSize:size]];
        }
        if (is_centered) [label setAlignment:NSTextAlignmentCenter];
        
        NSStackView* parent = current_stack();
        if (parent) {
            [parent addView:label inGravity:NSStackViewGravityTop];
        } else if (global_view) {
            [global_view addSubview:label];
        }
    });
}

void __sys_draw_button(const char* label, long x, long y, long w, long h, long callback_id) {
    run_on_main_async(^{
        NSButton* btn = [NSButton buttonWithTitle:[NSString stringWithUTF8String:label] target:nil action:nil];
        [btn setBezelStyle:NSBezelStyleRounded];
        
        NSStackView* parent = current_stack();
        if (parent) {
            [parent addView:btn inGravity:NSStackViewGravityTop];
        } else if (global_view) {
            [global_view addSubview:btn];
        }
    });
}

void __sys_draw_textfield(const char* placeholder, long x, long y, long w, long h, long __unused id) {
    run_on_main_async(^{
        NSTextField* tf = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, w > 0 ? w : 200, 24)];
        [tf setPlaceholderString:[NSString stringWithUTF8String:placeholder]];
        [tf setBezelStyle:NSTextFieldSquareBezel];
        
        NSStackView* parent = current_stack();
        if (parent) {
            [parent addView:tf inGravity:NSStackViewGravityTop];
        } else if (global_view) {
            [global_view addSubview:tf];
        }
    });
}

void __sys_draw_image(const char* path, long x, long y, long w, long h) {
    run_on_main_async(^{
        NSString* p = [NSString stringWithUTF8String:path];
        NSImage* img = [[NSImage alloc] initWithContentsOfFile:p];
        if (img) {
            NSImageView* iv = [NSImageView imageViewWithImage:img];
            [iv setFrameSize:NSMakeSize(w > 0 ? w : 100, h > 0 ? h : 100)];
            [iv setImageScaling:NSImageScaleProportionallyUpOrDown];
            
            NSStackView* parent = current_stack();
            if (parent) {
                [parent addView:iv inGravity:NSStackViewGravityTop];
            } else if (global_view) {
                [global_view addSubview:iv];
            }
        }
    });
}

void __sys_run_gui() {
    [NSApp run];
}

void __sys_reset_cursor() {}

void __sys_clear_view() {
    run_on_main_async(^{
        if (view_stack) {
            [view_stack removeAllObjects];
        }
        NSArray* subviews = [global_view subviews];
        for (NSView* v in [subviews copy]) {
            [v removeFromSuperview];
        }
    });
}

// --- Utils (Provided by runtime_core.cpp) ---
// void __sys_print(long n);
// void __sys_println(const char* s);
// void __sys_exit(long code);
// Compiler Internal Aliases (Moved to end to ensure symbols exist)
void end_stack() { __sys_end_stack(); }


