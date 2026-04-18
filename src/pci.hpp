#pragma once

#include <string>

struct PciGpuName {
    std::string vendor_name;
    std::string device_name;
    bool found = false;
};

PciGpuName lookup_pci_name(const std::string& vendor_id, const std::string& device_id);
