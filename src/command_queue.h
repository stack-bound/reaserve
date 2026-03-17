#pragma once

#include <string>
#include <deque>
#include <mutex>
#include <memory>
#include <future>
#include <nlohmann/json.hpp>

namespace reaserve {

using json = nlohmann::json;

struct PendingCommand {
    std::string method;
    json params;
    json id;
    std::promise<std::string> response;
};

class CommandQueue {
public:
    // Push a command and return a future for the response.
    // Called from TCP threads.
    std::future<std::string> push(const std::string& method, const json& params, const json& id);

    // Try to pop a command. Returns nullptr if queue is empty.
    // Called from the REAPER main thread timer.
    std::unique_ptr<PendingCommand> try_pop();

    // Number of pending commands.
    size_t size() const;

private:
    mutable std::mutex mutex_;
    std::deque<std::unique_ptr<PendingCommand>> queue_;
};

} // namespace reaserve
