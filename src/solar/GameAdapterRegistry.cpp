#include "solar/GameAdapterRegistry.hpp"

#include "solar/CupheadAdapter.hpp"
#include "solar/Logger.hpp"
#include "solar/MonoBridge.hpp"
#include "solar/NativeHookRegistry.hpp"
#include "solar/TitleManager.hpp"

namespace Solar::GameAdapterRegistry {

void Reset() {
    NativeHookRegistry::ClearRegistrations();
    MonoBridge::ResetObservations();
}

bool PrepareForTitle(uint64_t titleId) {
    Reset();

    if (CupheadAdapter::Supports(titleId)) {
        const bool ok = CupheadAdapter::RegisterHooks();
        Logger::Info("Game Adapter: Cuphead %s for title %s",
                     ok ? "registered" : "registration failed",
                     TitleManager::FormatTitleId(titleId).c_str());
        return ok;
    }

    Logger::Info("Game Adapter: no built-in adapter for title %s",
                 TitleManager::FormatTitleId(titleId).c_str());
    return false;
}

} // namespace Solar::GameAdapterRegistry
