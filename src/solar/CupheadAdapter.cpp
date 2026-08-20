#include "solar/CupheadAdapter.hpp"

#include "solar/Logger.hpp"
#include "solar/MonoBridge.hpp"

namespace Solar::CupheadAdapter {
namespace {

constexpr uint64_t SupportedTitleIds[] = {TitleId};

} // namespace

bool Supports(uint64_t titleId) {
    return titleId == TitleId;
}

bool RegisterHooks() {
    MonoBridge::CompileHookTarget target;
    target.titleIds = SupportedTitleIds;
    target.titleIdCount = 1;
    target.versionMin = SupportedVersion;
    target.versionMax = SupportedVersion;
    target.executableName = ExecutableName;

    const bool registered = MonoBridge::RegisterCompileTraceHook(MonoTraceHookId, target);
    if (registered) {
        Logger::Info("Cuphead Adapter v0.1: Mono trace hook ready; reserved Player 3 id=%d",
                     PlayerThreeId);
    }
    return registered;
}

} // namespace Solar::CupheadAdapter
