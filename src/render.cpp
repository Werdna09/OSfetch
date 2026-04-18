#include "render.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace ansi {
const std::string reset  = "\033[0m";
const std::string bold   = "\033[1m";
const std::string accent = "\033[38;2;226;140;140m";
const std::string blue   = "\033[38;2;164;185;239m";
const std::string green  = "\033[38;2;179;225;163m";
const std::string yellow = "\033[38;2;234;221;160m";
const std::string pink   = "\033[38;2;240;175;225m";
const std::string fg     = "\033[38;2;215;218;224m";
const std::string muted  = "\033[38;2;110;108;124m";
}

namespace layout {
constexpr int min_logo_width    = 22;
constexpr int gap               = 2;
constexpr int min_right_width   = 20;
constexpr int max_box_width     = 70;
constexpr int min_value_width   = 4;
constexpr int label_width       = 10;
constexpr int value_offset      = 18;
}

std::string truncate_text(const std::string& text, std::size_t max_len) {
    if (text.size() <= max_len) return text;
    if (max_len <= 3) return text.substr(0, max_len);
    return text.substr(0, max_len - 3) + "...";
}

std::string repeat_utf8(const std::string& s, int count) {
    std::string out;
    for (int i = 0; i < count; ++i) {
        out += s;
    }
    return out;
}

static int get_logo_width(const std::vector<std::string>& logo) {
    std::size_t max_width = 0;
    for (const auto& line : logo) {
        max_width = std::max(max_width, line.size());
    }
    return std::max(layout::min_logo_width, static_cast<int>(max_width));
}

static std::string make_box_line(const std::string& left,
                                 const std::string& fill,
                                 const std::string& right,
                                 int width) {
    if (width < 2) width = 2;

    return ansi::muted + left
         + repeat_utf8(fill, width - 2)
         + right
         + ansi::reset;
}

std::string make_box_top(int width) {
    return make_box_line("┏", "━", "┓", width);
}

std::string make_box_bottom(int width) {
    return make_box_line("┗", "━", "┛", width);
}

std::string box_line(const std::string& color,
                     const std::string& label,
                     const std::string& value,
                     std::size_t value_width) {
    std::ostringstream out;
    out << ansi::muted << "┃" << ansi::reset
        << "  " << color << "██" << ansi::reset
        << " " << ansi::fg << ansi::bold
        << std::left << std::setw(layout::label_width) << (label + ":")
        << ansi::reset
        << " " << truncate_text(value, value_width);

    return out.str();
}

std::vector<std::string> build_info_lines(const SystemInfo& sys,
                                          int box_width,
                                          std::size_t value_width) {
    const std::string user_host = sys.user + "@" + sys.hostname;

    std::vector<std::string> lines = {
        "",
        ansi::accent + ansi::bold + user_host + ansi::reset,
        "",
        make_box_top(box_width),
        box_line(ansi::accent, "OS", sys.os, value_width),
        box_line(ansi::blue, "Kernel", sys.kernel, value_width),
        box_line(ansi::green, "Uptime", sys.uptime, value_width),
        box_line(ansi::yellow, "Shell", sys.shell, value_width),
        box_line(ansi::pink, "Packages", sys.packages, value_width),
        box_line(ansi::blue, "WM", sys.wm, value_width),
        box_line(ansi::green, "Terminal", sys.terminal, value_width),
        make_box_bottom(box_width),

        make_box_top(box_width),
        box_line(ansi::yellow, "CPU", sys.cpu, value_width),
    };
    for (std::size_t i = 0; i < sys.gpus.size(); ++i) {
        std::string label = (sys.gpus.size() == 1) ? "GPU" : "GPU " + std::to_string(i + 1);
        lines.push_back(box_line(ansi::accent, label, sys.gpus[i], value_width));
    }

    lines.push_back(box_line(ansi::blue, "RAM", sys.ram, value_width));
    lines.push_back(box_line(ansi::green, "Disk", sys.disk, value_width));
    lines.push_back(box_line(ansi::pink, "Battery", sys.battery, value_width));
    lines.push_back(make_box_bottom(box_width));

    return lines;
}

void render_fetch(const std::vector<std::string>& logo, const SystemInfo& sys) {
    const std::vector<std::string> logo_colors = {
        ansi::blue, ansi::pink, ansi::green, ansi::yellow,
        ansi::accent, ansi::blue, ansi::pink, ansi::green
    };

    const int logo_width  = get_logo_width(logo);
    const int term_width  = get_terminal_width();
    const int right_width = std::clamp(
            term_width - logo_width - layout::gap,
            layout::min_right_width,
            layout::max_box_width
            );

    const int box_width   = right_width;
    const int value_width = std::max(layout::min_value_width,
                                     right_width - layout::value_offset);

    const auto info = build_info_lines(sys, box_width, value_width);

    int logo_pad_top = 0;
    if (logo.size() < info.size()) {
        logo_pad_top = static_cast<int>(info.size() - logo.size()) / 2;
    }

    int rows = static_cast<int>(info.size());
    if (static_cast<int>(logo.size()) + logo_pad_top > rows) {
        rows = static_cast<int>(logo.size()) + logo_pad_top;
    }

    std::cout << '\n';
    for (int i = 0; i < rows; ++i) {
        const int logo_index = i - logo_pad_top;

        std::string left;
        std::string color = ansi::fg;

        if (logo_index >= 0 && logo_index < static_cast<int>(logo.size())) {
            left = logo[logo_index];
            color = logo_colors[logo_index % logo_colors.size()];
        }

        const std::string right =
            (i < static_cast<int>(info.size())) ? info[i] : "";

        std::cout << color
                  << std::left << std::setw(logo_width) << left
                  << ansi::reset
                  << std::string(layout::gap, ' ')
                  << right
                  << '\n';
    }
    std::cout << '\n';
}
