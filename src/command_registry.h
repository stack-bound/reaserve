#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace reaserve {

using json = nlohmann::json;
using CommandHandler = std::function<json(const json& params)>;

class CommandRegistry {
public:
    void register_command(const std::string& name, CommandHandler handler);
    bool has_command(const std::string& name) const;
    json dispatch(const std::string& name, const json& params) const;
    std::vector<std::string> list_commands() const;

private:
    std::unordered_map<std::string, CommandHandler> handlers_;
};

} // namespace reaserve
