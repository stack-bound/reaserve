#include "command_registry.h"
#include "version.h"

namespace reaserve {
namespace commands {

void register_ping(CommandRegistry& registry) {
    registry.register_command("ping", [](const json& /*params*/) -> json {
        return {
            {"pong", true},
            {"version", REASERVE_VERSION}
        };
    });
}

} // namespace commands
} // namespace reaserve
