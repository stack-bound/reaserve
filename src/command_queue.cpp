#include "command_queue.h"

namespace reaserve {

std::future<std::string> CommandQueue::push(const std::string& method, const json& params, const json& id) {
    auto cmd = std::make_unique<PendingCommand>();
    cmd->method = method;
    cmd->params = params;
    cmd->id = id;

    auto future = cmd->response.get_future();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(cmd));
    }

    return future;
}

std::unique_ptr<PendingCommand> CommandQueue::try_pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return nullptr;

    auto cmd = std::move(queue_.front());
    queue_.pop_front();
    return cmd;
}

size_t CommandQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

} // namespace reaserve
