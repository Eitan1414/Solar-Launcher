#include "solar/CupheadAdapter.hpp"

#include "solar/CupheadPlayer3.hpp"
#include "solar/Logger.hpp"

namespace Solar::CupheadAdapter {

bool Supports(uint64_t titleId) {
    return titleId == TitleId;
}

bool RegisterHooks() {
    const bool registered = CupheadPlayer3::RegisterHook(MonoTraceHookId);
    if (registered) {
        Logger::Info("Cuphead Player 3 Test 2: runtime hook ready; Player 3 id=%d",
                     PlayerThreeId);
    } else {
        Logger::Warn("Cuphead Player 3 Test 2: runtime hook registration failed");
    }
    return registered;
}

} // namespace Solar::CupheadAdapter
