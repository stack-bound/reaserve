#pragma once

#include <string>

namespace reaserve {

// RAII undo block. Calls Undo_BeginBlock on construction,
// Undo_EndBlock on destruction.
class UndoBlock {
public:
    explicit UndoBlock(const std::string& description, int flags = -1);
    ~UndoBlock();

    UndoBlock(const UndoBlock&) = delete;
    UndoBlock& operator=(const UndoBlock&) = delete;

private:
    std::string description_;
    int flags_;
};

} // namespace reaserve
