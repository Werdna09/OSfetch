#pragma once
#include <string>
#include <vector>

struct SystemInfo {
    std::string user;
    std::string hostname;
    std::string shell;
    std::string terminal;
    std::string wm;
    std::string kernel;
    std::string os;
    std::string uptime;
    std::string packages;
    std::string cpu;
    std::vector<std::string> gpus;
    std::string ram;
    std::string disk;
    std::string battery;
};

std::string get_user();
std::string get_shell();
std::string get_terminal();
std::string get_wm();
std::string get_hostname_str();
std::string get_kernel();
std::string get_os_pretty_name();
std::string get_uptime();
std::string get_packages();
std::string get_cpu();
std::vector<std::string> get_gpus();
std::string get_ram();
std::string get_disk();
std::string get_battery();

SystemInfo collect_system_info();
int get_terminal_width();
