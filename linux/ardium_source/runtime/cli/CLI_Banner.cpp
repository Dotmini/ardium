#include <iostream>
#include <string>

/**
 * ============================================================================
 *  ARDIUM v5.0 - CLI BRANDING COMPONENT
 * ============================================================================
 *  Architect: Major
 *  Branding:  Dotmini Software Enterprise
 * ============================================================================
 */

void print_banner() {
    // ANSI Color Codes
    const std::string CYAN = "\033[1;36m";
    const std::string YELLOW = "\033[1;33m";
    const std::string RESET = "\033[0m";
    const std::string BOLD = "\033[1m";

    std::cout << CYAN << R"(
    █████  ██████  ██████  ██ ██    ██ ███    ███ 
   ██   ██ ██   ██ ██   ██ ██ ██    ██ ████  ████ 
   ███████ ██████  ██   ██ ██ ██    ██ ██ ████ ██ 
   ██   ██ ██   ██ ██   ██ ██ ██    ██ ██  ██  ██ 
   ██   ██ ██   ██ ██████  ██  ██████  ██      ██ 
    )" << RESET << std::endl;

    std::cout << YELLOW << "   --- TITAN ENGINE v5.0 | SUPREME PERFORMANCE ---" << RESET << std::endl << std::endl;

    std::cout << BOLD << "   CREATOR : " << RESET << "Tirawat Nantamas" << std::endl;
    std::cout << BOLD << "   ROLE    : " << RESET << "Founder of Dotmini Software | Advisor of SPU AI CLUB" << std::endl;
    std::cout << BOLD << "   DEPT    : " << RESET << "School of Entrepreneurship SPU (SE)" << std::endl;
    std::cout << BOLD << "   MAJOR   : " << RESET << "Interdisciplinary Technology & Innovation" << std::endl;
    std::cout << std::string(66, '-') << std::endl;
}
