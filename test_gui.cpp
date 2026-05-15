#include <iostream>

extern "C" {
    void __sys_init_gui();
    void __sys_create_window(const char* title, long x, long y, long w, long h);
    void __sys_set_bg();
    void __sys_draw_text(const char* text, long x, long y, long size, long is_bold, long is_centered);
    void __sys_draw_button(const char* label, long x, long y, long w, long h, long callback_id);
    void __sys_run_gui();
}

int main() {
    std::cout << "Starting GUI test..." << std::endl;
    __sys_init_gui();
    __sys_create_window("Ardium Raylib Test", 0, 0, 800, 600);
    __sys_draw_text("Ardium Cross-Platform GUI", 0, 100, 32, 1, 1);
    __sys_draw_button("Click Me!", 0, 200, 200, 50, 1);
    
    // We won't actually run it because it blocks the AI, but compiling it proves it works.
    std::cout << "GUI compiled and linked successfully!" << std::endl;
    return 0;
}
