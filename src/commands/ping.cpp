#include "command_registry.h"

namespace reaserve {
namespace commands {

void register_ping(CommandRegistry& registry) {
    registry.register_command("ping", [](const json& /*params*/) -> json {
        return {
            {"pong", true},
            {"version", "0.1.0"}
        };
    });
}

} // namespace commands
} // namespace reaserve
