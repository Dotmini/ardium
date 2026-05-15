#include <iostream>

extern "C" void gpu_init();

int main() {
    std::cout << "Starting Graphics Test..." << std::endl;
    gpu_init();
    std::cout << "Graphics Test Completed." << std::endl;
    return 0;
}
