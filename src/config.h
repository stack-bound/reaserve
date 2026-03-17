#pragma once

#include <string>

namespace reaserve {

struct Config {
    std::string bind = "0.0.0.0";
    int port = 9876;
};

// Load config from reaserve.ini in REAPER's resource path.
// If the file doesn't exist, creates it with defaults.
// resource_path should come from GetResourcePath().
Config load_config(const std::string& resource_path);

} // namespace reaserve
