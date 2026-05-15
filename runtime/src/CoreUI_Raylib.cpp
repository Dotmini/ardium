// runtime/src/CoreUI_Raylib.cpp
#include "raylib.h"
#include <iostream>
#include <string>
#include <vector>

// Prevent C++ mangling
extern "C" {

    // --- Layout State ---
    static long g_cursor_y = 50;
    static long g_window_width = 800;
    static long g_window_height = 600;

    struct UIElement {
        int type; // 0=Text, 1=Button, 2=TextField
        std::string text;
        long x, y, w, h, size, extra;
    };
    static std::vector<UIElement> g_ui_elements;

    void __sys_init_gui() {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        g_ui_elements.clear();
    }

    void __sys_create_window(const char* title, long x, long y, long w, long h) {
        g_window_width = w;
        g_window_height = h;
        InitWindow(w, h, title);
    }

    void __sys_begin_vstack(long spacing, long alignment) {}
    void __sys_begin_hstack(long spacing, long alignment) {}
    void __sys_begin_zstack() {}
    void __sys_end_stack() {}
    
    void __sys_set_bg() {} // Handled in render loop

    void __sys_reset_cursor() {
        g_cursor_y = 50;
    }

    void __sys_draw_text(const char* text, long x, long y, long size, long is_bold, long is_centered) {
        long actual_y = (y == 0) ? g_cursor_y : y;
        long actual_x = (x == 0) ? 50 : x;
        g_ui_elements.push_back({0, text, actual_x, actual_y, 0, 0, size, is_centered});
        if (y == 0) g_cursor_y += size + 10;
    }

    void __sys_draw_button(const char* label, long x, long y, long w, long h, long callback_id) {
        long actual_y = (y == 0) ? g_cursor_y : y;
        long actual_x = (x == 0) ? 50 : x;
        if (w == 0) w = 200;
        if (h == 0) h = 40;
        g_ui_elements.push_back({1, label, actual_x, actual_y, w, h, 20, callback_id});
        if (y == 0) g_cursor_y += h + 20;
    }

    void __sys_draw_textfield(const char* placeholder, long x, long y, long w, long h, long id) {
        long actual_y = (y == 0) ? g_cursor_y : y;
        long actual_x = (x == 0) ? 50 : x;
        if (w == 0) w = 300;
        if (h == 0) h = 40;
        g_ui_elements.push_back({2, placeholder, actual_x, actual_y, w, h, 20, id});
        if (y == 0) g_cursor_y += h + 20;
    }

    void __sys_draw_image(const char* path, long x, long y, long w, long h) {
        long actual_y = (y == 0) ? g_cursor_y : y;
        g_ui_elements.push_back({0, "[IMAGE STUB]", x == 0 ? 50 : x, actual_y, 0, 0, 20, 0});
        if (y == 0) g_cursor_y += 100;
    }

    void __sys_clear_view() {
        g_cursor_y = 50;
        g_ui_elements.clear();
    }

    void __sys_run_gui() {
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground((Color){ 10, 10, 12, 255 }); // Dark mode Apple-like background
            
            for (auto& el : g_ui_elements) {
                if (el.type == 0) { // Text
                    long actual_x = el.x;
                    if (el.extra) { // is_centered
                        int textWidth = MeasureText(el.text.c_str(), el.size);
                        actual_x = (GetScreenWidth() - textWidth) / 2;
                    }
                    DrawText(el.text.c_str(), actual_x, el.y, el.size, RAYWHITE);
                } 
                else if (el.type == 1) { // Button
                    Rectangle btnBounds = { (float)el.x, (float)el.y, (float)el.w, (float)el.h };
                    Vector2 mousePoint = GetMousePosition();
                    bool isHovered = CheckCollisionPointRec(mousePoint, btnBounds);
                    Color btnColor = isHovered ? (Color){ 50, 50, 60, 255 } : (Color){ 30, 30, 40, 255 };
                    
                    DrawRectangleRounded(btnBounds, 0.2f, 10, btnColor);
                    DrawRectangleRoundedLines(btnBounds, 0.2f, 10, 1, (Color){ 100, 100, 120, 255 });

                    int textWidth = MeasureText(el.text.c_str(), el.size);
                    DrawText(el.text.c_str(), el.x + (el.w - textWidth) / 2, el.y + (el.h - el.size) / 2, el.size, RAYWHITE);
                }
                else if (el.type == 2) { // TextField
                    Rectangle bounds = { (float)el.x, (float)el.y, (float)el.w, (float)el.h };
                    DrawRectangleRounded(bounds, 0.2f, 10, (Color){ 15, 15, 20, 255 });
                    DrawRectangleRoundedLines(bounds, 0.2f, 10, 1, (Color){ 80, 80, 90, 255 });
                    DrawText(el.text.c_str(), el.x + 15, el.y + (el.h - el.size) / 2, el.size, GRAY);
                }
            }
            EndDrawing();
        }
        CloseWindow();
    }

} // extern "C"
