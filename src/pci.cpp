#include "pci.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace {

std::string trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";

    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string strip_hex_prefix(std::string s) {
    s = trim(s);
    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
        s = s.substr(2);
    }
    for (char& c : s) {
        if (c >= 'A' && c <= 'F') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

std::vector<std::string> pci_ids_paths() {
    return {
        "/usr/share/hwdata/pci.ids",
        "/usr/share/misc/pci.ids"
    };
}

} // namespace

PciGpuName lookup_pci_name(const std::string& vendor_id, const std::string& device_id) {
    const std::string wanted_vendor = strip_hex_prefix(vendor_id);
    const std::string wanted_device = strip_hex_prefix(device_id);

    for (const auto& path : pci_ids_paths()) {
        std::ifstream file(path);
        if (!file) continue;

        std::string line;
        bool in_vendor_block = false;
        std::string vendor_name;

        while (std::getline(file, line)) {
            if (line.empty()) continue;
            if (line[0] == '#') continue;

            if (line[0] != '\t') {
                if (line.size() < 4) continue;

                const std::string id = strip_hex_prefix(line.substr(0, 4));
                if (id == wanted_vendor) {
                    vendor_name = trim(line.substr(4));
                    in_vendor_block = true;
                } else {
                    in_vendor_block = false;
                }
                continue;
            }

            if (!in_vendor_block) continue;

            if (line.size() < 5) continue;
            if (line.size() >= 2 && line[0] == '\t' && line[1] == '\t') continue;

            const std::string id = strip_hex_prefix(line.substr(1, 4));
            if (id == wanted_device) {
                return {
                    vendor_name,
                    trim(line.substr(5)),
                    true
                };
            }
        }
    }

    return {};
}
