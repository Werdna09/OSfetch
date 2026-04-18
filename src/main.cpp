#include "logo.hpp"
#include "render.hpp"
#include "system.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    try {
        const char* home = std::getenv("HOME");
        if (!home) {
            throw std::runtime_error("HOME is not set");
        }

        const std::string logo_path =
            std::string(home) + "/.config/scripts/fetch/assets/logo.txt";

        const auto logo = read_logo(logo_path);
        const auto sys = collect_system_info();
        render_fetch(logo, sys);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
