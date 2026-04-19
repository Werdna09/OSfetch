#include "render.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <chrono>
#include <thread>

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

// canvas / orbit
constexpr int orbit_canvas_width  = 30;
constexpr int orbit_canvas_height = 14;
}

using Canvas = std::vector<std::string>;

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

static Canvas make_canvas(int width, int height) {
    return Canvas(height, std::string(width, ' '));
}

static void draw_logo(Canvas& canvas,
                      const std::vector<std::string>& logo,
                      int start_x,
                      int start_y) {
    for (int y = 0; y < static_cast<int>(logo.size()); ++y) {
        const int cy = start_y + y;
        if (cy < 0 || cy >= static_cast<int>(canvas.size())) continue;

        for (int x = 0; x < static_cast<int>(logo[y].size()); ++x) {
            const int cx = start_x + x;
            if (cx < 0 || cx >= static_cast<int>(canvas[cy].size())) continue;

            if (logo[y][x] != '\0') {
                canvas[cy][cx] = logo[y][x];
            }
        }
    }
}

static void draw_block_if_empty(Canvas& canvas,
                                int x,
                                int y,
                                const std::vector<std::string>& shape) {
    for (int dy = 0; dy < static_cast<int>(shape.size()); ++dy) {
        const int cy = y + dy;
        if (cy < 0 || cy >= static_cast<int>(canvas.size())) continue;

        for (int dx = 0; dx < static_cast<int>(shape[dy].size()); ++dx) {
            const int cx = x + dx;
            if (cx < 0 || cx >= static_cast<int>(canvas[cy].size())) continue;

            const char ch = shape[dy][dx];
            if (ch == ' ') continue;

            if (canvas[cy][cx] == ' ') {
                canvas[cy][cx] = ch;
            }
        }
    }
}

struct OrbitPoint {
    int x;
    int y;
    double depth;
    std::vector<std::string> shape;
};

static void draw_block(Canvas& canvas,
                       int x,
                       int y,
                       const std::vector<std::string>& shape) {
    for (int dy = 0; dy < static_cast<int>(shape.size()); ++dy) {
        const int cy = y + dy;
        if (cy < 0 || cy >= static_cast<int>(canvas.size())) continue;

        for (int dx = 0; dx < static_cast<int>(shape[dy].size()); ++dx) {
            const int cx = x + dx;
            if (cx < 0 || cx >= static_cast<int>(canvas[cy].size())) continue;

            const char ch = shape[dy][dx];
            if (ch == ' ') continue;

            canvas[cy][cx] = ch;
        }
    }
}

static std::vector<std::string> build_orbit_lines(const std::vector<std::string>& logo, double t) {
    Canvas canvas = make_canvas(layout::orbit_canvas_width, layout::orbit_canvas_height);

    const int logo_width = [&]() {
        int w = 0;
        for (const auto& line : logo) {
            w = std::max(w, static_cast<int>(line.size()));
        }
        return w;
    }();

    const int logo_height = static_cast<int>(logo.size());

    const int logo_x = (layout::orbit_canvas_width  - logo_width)  / 2;
    const int logo_y = (layout::orbit_canvas_height - logo_height) / 2;

    const double cx = layout::orbit_canvas_width  / 2.0;
    const double cy = layout::orbit_canvas_height / 2.0;

    const double a = 14.0;      // vodorovná poloosa
    const double b = 2.6;       // svislá poloosa
    const double rot = -0.20;   // náklon elipsy v radiánech

    const double cos_r = std::cos(rot);
    const double sin_r = std::sin(rot);

    constexpr int point_count = 8;
    const double pi = 3.14159265358979323846;

    std::vector<OrbitPoint> points;

    for (int i = 0; i < point_count; ++i) {
        const double theta = t + (2.0 * pi * i) / point_count;

        const double ex = a * std::cos(theta);
        const double ey = b * std::sin(theta);

        const double rx = ex * cos_r - ey * sin_r;
        const double ry = ex * sin_r + ey * cos_r;

        const int px = static_cast<int>(std::lround(cx + rx));
        const int py = static_cast<int>(std::lround(cy + ry));

        const double depth = std::sin(theta);

        std::vector<std::string> shape;
        if (depth > 0.35) {
            shape = {
                "@@",
                "@@"
            };
        } else if (depth < -0.35) {
            shape = {
                "..",
                ".."
            };
        } else {
            shape = {
                "**",
                "**"
            };
        }

        points.push_back({px - 1, py - 1, depth, shape});
    }

    // 1) zadní část orbitu
    for (const auto& p : points) {
        if (p.depth <= 0.0) {
            draw_block_if_empty(canvas, p.x, p.y, p.shape);
        }
    }

    // 2) logo doprostřed
    draw_logo(canvas, logo, logo_x, logo_y);

    // 3) přední část orbitu přes logo
    for (const auto& p : points) {
        if (p.depth > 0.0) {
            draw_block(canvas, p.x, p.y, p.shape);
        }
    }

    return canvas;
}

static int get_left_width(const std::vector<std::string>& left_lines) {
    int max_width = 0;
    for (const auto& line : left_lines) {
        max_width = std::max(max_width, static_cast<int>(line.size()));
    }
    return std::max(layout::min_logo_width, max_width);
}

static std::string colorize_left_line(const std::string& line) {
    std::string out;

    for (char ch : line) {
        if (ch == '@') {
            out += ansi::accent + std::string(1, ch) + ansi::reset;
        } else if (ch == '*') {
            out += ansi::blue + std::string(1, ch) + ansi::reset;
        } else if (ch == '.') {
            out += ansi::muted + std::string(1, ch) + ansi::reset;
        } else if (ch == ' ') {
            out += ' ';
        } else {
            out += ansi::fg + std::string(1, ch) + ansi::reset;
        }
    }

    return out;
}

void render_fetch(const std::vector<std::string>& logo, const SystemInfo& sys) {
    const auto left_lines = build_orbit_lines(logo, 0.0);

    const int left_width  = get_left_width(left_lines);
    const int term_width  = get_terminal_width();
    const int right_width = std::clamp(
        term_width - left_width - layout::gap,
        layout::min_right_width,
        layout::max_box_width
    );

    const int box_width   = right_width;
    const int value_width = std::max(layout::min_value_width,
                                     right_width - layout::value_offset);

    const auto info = build_info_lines(sys, box_width, value_width);

    int left_pad_top = 0;
    if (left_lines.size() < info.size()) {
        left_pad_top = static_cast<int>(info.size() - left_lines.size()) / 2;
    }

    int rows = static_cast<int>(info.size());
    if (static_cast<int>(left_lines.size()) + left_pad_top > rows) {
        rows = static_cast<int>(left_lines.size()) + left_pad_top;
    }

    std::cout << '\n';
    for (int i = 0; i < rows; ++i) {
        const int left_index = i - left_pad_top;

        std::string left;
        if (left_index >= 0 && left_index < static_cast<int>(left_lines.size())) {
            left = left_lines[left_index];
        }

        const std::string right =
            (i < static_cast<int>(info.size())) ? info[i] : "";
            
        std::string padded_left = left;
        if (static_cast<int>(padded_left.size()) < left_width) {
            padded_left += std::string(left_width - padded_left.size(), ' ');
        }

        std::cout   << colorize_left_line(padded_left)
                    << std::string(layout::gap, ' ')
                    << right
                    << '\n';
        }
    std::cout << '\n';
}

void render_fetch_animated(const std::vector<std::string>& logo, const SystemInfo& sys) {
    std::cout << "\033[2J\033[H";   // clear screen + home
    std::cout << "\033[?25l";       // hide cursor

    double t = 0.0;
    const int term_width = get_terminal_width();

    while (true) {
        std::cout << "\033[H"; // move cursor to top-left

        const auto left_lines = build_orbit_lines(logo, t);

        const int left_width  = get_left_width(left_lines);
        const int right_width = std::clamp(
            term_width - left_width - layout::gap,
            layout::min_right_width,
            layout::max_box_width
        );

        const int box_width   = right_width;
        const int value_width = std::max(layout::min_value_width,
                                         right_width - layout::value_offset);

        const auto info = build_info_lines(sys, box_width, value_width);

        int left_pad_top = 0;
        if (left_lines.size() < info.size()) {
            left_pad_top = static_cast<int>(info.size() - left_lines.size()) / 2;
        }

        int rows = static_cast<int>(info.size());
        if (static_cast<int>(left_lines.size()) + left_pad_top > rows) {
            rows = static_cast<int>(left_lines.size()) + left_pad_top;
        }

        for (int i = 0; i < rows; ++i) {
            const int left_index = i - left_pad_top;

            std::string left;
            if (left_index >= 0 && left_index < static_cast<int>(left_lines.size())) {
                left = left_lines[left_index];
            }

            const std::string right =
                (i < static_cast<int>(info.size())) ? info[i] : "";

            std::string padded_left = left;
            if (static_cast<int>(padded_left.size()) < left_width) {
                padded_left += std::string(left_width - padded_left.size(), ' ');
            }
        
            std::cout   << colorize_left_line(padded_left)
                        << std::string(layout::gap, ' ')
                        << right
                        << '\n';
        }   
            

        std::cout.flush();

        t += 0.10;
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }

    // prakticky se sem nedostaneš, protože loop je nekonečný
    std::cout << "\033[?25h";
}
