#pragma once
#include <string>
#include <vector>
#include "system.hpp"

namespace ansi {
    extern const std::string reset;
    extern const std::string bold;
    extern const std::string accent;
    extern const std::string blue;
    extern const std::string green;
    extern const std::string yellow;
    extern const std::string pink;
    extern const std::string fg;
    extern const std::string muted;
}

std::string truncate_text(const std::string& text, std::size_t max_len);
std::string repeat_utf8(const std::string& s, int count);

std::string make_box_top(int width);
std::string make_box_bottom(int width);

std::string box_line(const std::string& color,
                     const std::string& label,
                     const std::string& value,
                     std::size_t value_width);

std::vector<std::string> build_info_lines(const SystemInfo& sys, int box_width, std::size_t value_width);

void render_fetch(const std::vector<std::string>& logo, const SystemInfo& sys);
