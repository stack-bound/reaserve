#include "undo.h"
#include "reaper_api.h"

namespace reaserve {

UndoBlock::UndoBlock(const std::string& description, int flags)
    : description_(description), flags_(flags) {
    if (api::Undo_BeginBlock) {
        api::Undo_BeginBlock();
    }
}

UndoBlock::~UndoBlock() {
    if (api::Undo_EndBlock) {
        api::Undo_EndBlock(description_.c_str(), flags_);
    }
}

} // namespace reaserve
