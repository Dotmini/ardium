/* runtime_gui_stubs.cpp - Cross-platform stubs for macOS GUI functions */

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Global State for Layout ---
long __native_cursor_x = 0;
long __native_cursor_y = 0;
long __native_layout_mode = 0;

long __agui_get_cursor_x() { return __native_cursor_x; }
void __agui_set_cursor_x(long v) { __native_cursor_x = v; }

long __agui_get_cursor_y() { return __native_cursor_y; }
void __agui_set_cursor_y(long v) { __native_cursor_y = v; }

long __agui_get_layout_mode() { return __native_layout_mode; }
void __agui_set_layout_mode(long v) { __native_layout_mode = v; }

// --- Layout Primitives ---
void __sys_begin_vstack(long spacing, long alignment) {
    printf("[GUI Stub] Begin VStack (spacing: %ld, alignment: %ld)\n", spacing, alignment);
}
void begin_vstack() { __sys_begin_vstack(8, 0); }

void __sys_begin_hstack(long spacing, long alignment) {
    printf("[GUI Stub] Begin HStack (spacing: %ld, alignment: %ld)\n", spacing, alignment);
}
void begin_hstack() { __sys_begin_hstack(8, 0); }

void __sys_set_bg() {
    printf("[GUI Stub] Set Background Color\n");
}

void __sys_begin_zstack() {
    printf("[GUI Stub] Begin ZStack\n");
}

void __sys_end_stack() {
    printf("[GUI Stub] End Stack\n");
}
void end_stack() { __sys_end_stack(); }

// --- GUI Implementation ---
void __sys_init_gui() {
    printf("[GUI Stub] Init GUI (Not supported natively on this OS)\n");
}

void __sys_create_window(const char* title, long x, long y, long w, long h) {
    printf("[GUI Stub] Create Window: '%s' (%ldx%ld)\n", title, w, h);
}

void __sys_draw_text(const char* text, long x, long y, long size, long is_bold, long is_centered) {
    printf("[GUI Stub] Draw Text: '%s'\n", text);
}

void __sys_draw_button(const char* label, long x, long y, long w, long h, long callback_id) {
    printf("[GUI Stub] Draw Button: '%s'\n", label);
}

void __sys_draw_textfield(const char* placeholder, long x, long y, long w, long h, long id) {
    printf("[GUI Stub] Draw TextField: '%s'\n", placeholder);
}

void __sys_draw_image(const char* path, long x, long y, long w, long h) {
    printf("[GUI Stub] Draw Image: '%s'\n", path);
}

void __sys_run_gui() {
    printf("[GUI Stub] Run GUI Event Loop\n");
}

void __sys_reset_cursor() {}

void __sys_clear_view() {
    printf("[GUI Stub] Clear View\n");
}

#ifdef __cplusplus
} // extern "C"
#endif
