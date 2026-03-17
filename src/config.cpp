#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace reaserve {

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

Config load_config(const std::string& resource_path) {
    Config cfg;
    std::string path = resource_path + "/reaserve.ini";

    std::ifstream file(path);
    if (!file.is_open()) {
        // Write defaults
        std::ofstream out(path);
        if (out.is_open()) {
            out << "[reaserve]\n";
            out << "port=9876\n";
            out << "bind=0.0.0.0\n";
        }
        return cfg;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (key == "port") {
            try { cfg.port = std::stoi(val); } catch (...) {}
        } else if (key == "bind") {
            cfg.bind = val;
        }
    }

    return cfg;
}

} // namespace reaserve
