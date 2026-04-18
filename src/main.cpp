#include "logo.hpp"
#include "render.hpp"
#include "system.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        const auto logo = read_logo("assets/logo.txt");
        const auto sys = collect_system_info();
        render_fetch(logo, sys);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
