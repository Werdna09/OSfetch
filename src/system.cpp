#include "system.hpp"
#include "pci.hpp"

#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <array>
#include <cstdio>
#include <memory>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <algorithm>

#include <chrono>
#include <iostream>

template <typename F>
auto measure_ms(const char* name, F&& fn) {
    auto start = std::chrono::steady_clock::now();
    auto result = fn();
    auto end = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cerr << "[osfetch] " << name << ": " << ms << " ms\n";
    return result;
}

std::string trim(std::string s) {
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
    return s;
}

std::string read_first_line(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";

    std::string line;
    std::getline(file, line);
    return trim(line);
}

std::string get_user() {
    const char* user = std::getenv("USER");
    return user ? std::string(user) : "unknown";
}

std::string exec_command(const std::string& cmd) {
    std::array<char, 256> buffer{};
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        result += buffer.data();
    }

    pclose(pipe);

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    return result;
}

std::string get_shell() {
    const char* shell = std::getenv("SHELL");
    if (!shell) return "unknown";

    std::string full(shell);
    auto pos = full.find_last_of('/');
    std::string name = (pos == std::string::npos) ? full : full.substr(pos + 1);

    std::string version_line = exec_command(name + " --version 2>/dev/null | head -n 1");
    if (version_line.empty()) return name;

    if (name == "zsh") {
        auto p = version_line.find("zsh ");
        if (p != std::string::npos) {
            return version_line.substr(p);
        }
    }

    return version_line;
}

std::string get_terminal() {
    const char* term = std::getenv("TERM_PROGRAM");
    if (term && std::string(term).size() > 0) {
        return std::string(term);
    }

    const char* term_alt = std::getenv("TERMINAL");
    if (term_alt && std::string(term_alt).size() > 0) {
        return std::string(term_alt);
    }

    const char* term_env = std::getenv("TERM");
    if (term_env && std::string(term_env).size() > 0) {
        return std::string(term_env);
    }

    return "unknown";
}

std::string get_wm() {
    const char* wm = std::getenv("XDG_CURRENT_DESKTOP");
    if (wm) return wm;

    // fallback
    if (system("pgrep -x bspwm > /dev/null") == 0)
        return "bspwm";

    return "unknown";
}

std::string get_packages() {
    namespace fs = std::filesystem;

    const fs::path pacman_db("/var/lib/pacman/local");

    try {
        if (!fs::exists(pacman_db) || !fs::is_directory(pacman_db)) {
            return "unknown";
        }

        std::size_t count = 0;
        for (const auto& entry : fs::directory_iterator(pacman_db)) {
            if (entry.is_directory()) {
                ++count;
            }
        }

        return std::to_string(count) + " (pacman)";
    } catch (...) {
        return "unknown";
    }
}

std::string get_hostname_str() {
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return "unknown";
    }
    return std::string(hostname);
}

std::string get_kernel() {
    utsname uts {};
    if (uname(&uts) != 0) {
        return "unknown";
    }
    return std::string(uts.release);
}

std::string get_os_pretty_name() {
    std::ifstream file("/etc/os-release");
    if (!file) return "unknown";

    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("PRETTY_NAME=", 0) == 0) {
            std::string value = line.substr(12);
            if (!value.empty() && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            return value;
        }
    }

    return "unknown";
}

std::string get_uptime() {
    std::ifstream file("/proc/uptime");
    if (!file) return "unknown";

    double seconds = 0.0;
    file >> seconds;

    long total = static_cast<long>(seconds);
    long days = total / 86400;
    total %= 86400;
    long hours = total / 3600;
    total %= 3600;
    long minutes = total / 60;

    std::ostringstream out;
    bool first = true;

    if (days > 0) {
        out << days << " day" << (days == 1 ? "" : "s");
        first = false;
    }
    if (hours > 0) {
        if (!first) out << ", ";
        out << hours << " hour" << (hours == 1 ? "" : "s");
        first = false;
    }
    if (minutes > 0 || first) {
        if (!first) out << ", ";
        out << minutes << " minute" << (minutes == 1 ? "" : "s");
    }

    return out.str();
}

std::string get_ram() {
    std::ifstream file("/proc/meminfo");
    if (!file) return "unknown";

    long total = 0;
    long available = 0;

    std::string key;
    long value;
    std::string unit;

    while (file >> key >> value >> unit) {
        if (key == "MemTotal:") total = value;
        if (key == "MemAvailable:") {
            available = value;
            break;
        }
    }

    if (total <= 0 || available < 0) return "unknown";

    long used = total - available;

    auto to_gib = [](long kb) {
        return kb / 1024.0 / 1024.0;
    };

    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    out << to_gib(used) << " GiB / " << to_gib(total) << " GiB";

    return out.str();
}

std::string get_disk() {
    struct statvfs stat {};
    if (statvfs("/", &stat) != 0) return "unknown";

    unsigned long long total =
        static_cast<unsigned long long>(stat.f_blocks) * stat.f_frsize;
    unsigned long long free =
        static_cast<unsigned long long>(stat.f_bavail) * stat.f_frsize;
    unsigned long long used = total - free;

    auto to_gib = [](unsigned long long bytes) {
        return bytes / 1024.0 / 1024.0 / 1024.0;
    };

    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    out << to_gib(used) << " GiB / " << to_gib(total) << " GiB";

    return out.str();
}

std::string get_cpu() {
    std::ifstream file("/proc/cpuinfo");
    if (!file) return "unknown";

    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("model name", 0) == std::string::npos) continue;

        auto pos = line.find(':');
        if (pos == std::string::npos) continue;

        std::string value = line.substr(pos + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        return value;
    }

    return "unknown";
}

std::vector<std::string> get_gpus() {
    namespace fs = std::filesystem;

    std::vector<std::string> result;
    const fs::path drm_path("/sys/class/drm");

    try {
        if (!fs::exists(drm_path) || !fs::is_directory(drm_path)) {
            return result;
        }

        for (const auto& entry : fs::directory_iterator(drm_path)) {
            const std::string name = entry.path().filename().string();

            if (name.rfind("card", 0) != 0) continue;
            if (name.find('-') != std::string::npos) continue;

            const fs::path device_path = entry.path() / "device";
            if (!fs::exists(device_path)) continue;

            const std::string vendor = read_first_line((device_path / "vendor").string());
            const std::string device = read_first_line((device_path / "device").string());

            if (vendor.empty() || device.empty()) continue;

            const auto pci_name = lookup_pci_name(vendor, device);

            if (pci_name.found) {
                std::string pretty = pci_name.device_name;

                if (pretty.empty()) {
                    pretty = pci_name.vendor_name + " GPU";
                }

                result.push_back(pretty);
            } else {
                std::string label;

                if (vendor == "0x8086") {
                    label = "Intel GPU";
                } else if (vendor == "0x10de") {
                    label = "NVIDIA GPU";
                } else if (vendor == "0x1002") {
                    label = "AMD GPU";
                } else {
                    label = "Unknown GPU";
                }

                label += " (" + device + ")";
                result.push_back(label);
            }
        }
    } catch (...) {
        return result;
    }

    return result;
}

std::string get_battery() {
    namespace fs = std::filesystem;
    const fs::path base("/sys/class/power_supply");

    try {
        if (!fs::exists(base) || !fs::is_directory(base)) {
            return "unknown";
        }

        for (const auto& entry : fs::directory_iterator(base)) {
            if (!entry.is_directory()) continue;

            const auto type = read_first_line((entry.path() / "type").string());
            if (type != "Battery") continue;

            const auto capacity = read_first_line((entry.path() / "capacity").string());
            const auto status   = read_first_line((entry.path() / "status").string());

            if (capacity.empty()) return "unknown";
            if (status.empty()) return capacity + "%";

            return capacity + "% (" + status + ")";
        }
    } catch (...) {
        return "unknown";
    }

    return "unknown";
}

SystemInfo collect_system_info() {
    SystemInfo info;

    info.user      = get_user();
    info.hostname  = get_hostname_str();
    info.shell     = get_shell();
    info.terminal  = get_terminal();
    info.wm        = get_wm();
    info.kernel    = get_kernel();
    info.os        = get_os_pretty_name();
    info.uptime    = get_uptime();
    info.packages  = get_packages();
    info.gpus      = get_gpus();
    info.cpu       = get_cpu();
    info.ram       = get_ram();
    info.disk      = get_disk();
    info.battery   = get_battery();

    return info;
}

int get_terminal_width() {
    struct winsize w {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return static_cast<int>(w.ws_col);
    }
    return 100;
}
