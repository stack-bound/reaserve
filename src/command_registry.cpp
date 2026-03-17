#include "command_registry.h"
#include <stdexcept>

namespace reaserve {

void CommandRegistry::register_command(const std::string& name, CommandHandler handler) {
    handlers_[name] = std::move(handler);
}

bool CommandRegistry::has_command(const std::string& name) const {
    return handlers_.count(name) > 0;
}

json CommandRegistry::dispatch(const std::string& name, const json& params) const {
    auto it = handlers_.find(name);
    if (it == handlers_.end()) {
        throw std::runtime_error("Method not found: " + name);
    }
    return it->second(params);
}

std::vector<std::string> CommandRegistry::list_commands() const {
    std::vector<std::string> names;
    names.reserve(handlers_.size());
    for (const auto& [name, _] : handlers_) {
        names.push_back(name);
    }
    return names;
}

} // namespace reaserve
