#include "solar/NativeHookRegistry.hpp"
#include "solar/Logger.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace Solar::NativeHookRegistry {
namespace {

struct HookEntry {
    std::string id;
    function_replacement_data_t data;
};

std::vector<HookEntry> gEntries;

} // namespace

bool Register(const std::string &id, const function_replacement_data_t &data) {
    if (id.empty()) {
        return false;
    }

    auto it = std::find_if(gEntries.begin(), gEntries.end(), [&](const HookEntry &entry) {
        return entry.id == id;
    });

    if (it != gEntries.end()) {
        it->data = data;
        Logger::Info("Updated native hook registration: %s", id.c_str());
        return true;
    }

    gEntries.push_back({id, data});
    Logger::Info("Registered native hook: %s", id.c_str());
    return true;
}

bool Contains(const std::string &id) {
    return std::any_of(gEntries.begin(), gEntries.end(), [&](const HookEntry &entry) {
        return entry.id == id;
    });
}

ApplyResult Apply(const std::string &id, PatchedFunctionHandle *outHandle, bool *outInitiallyPatched) {
    auto it = std::find_if(gEntries.begin(), gEntries.end(), [&](const HookEntry &entry) {
        return entry.id == id;
    });

    if (it == gEntries.end()) {
        return ApplyResult::NotRegistered;
    }

    function_replacement_data_t data = it->data;
    bool initiallyPatched = false;
    PatchedFunctionHandle handle {};

    const FunctionPatcherStatus status = FunctionPatcher_AddFunctionPatch(
        &data, &handle, &initiallyPatched);

    if (status != FUNCTION_PATCHER_RESULT_SUCCESS) {
        Logger::Error("FunctionPatcher failed for hook %s: %s (%d)",
                      id.c_str(), FunctionPatcher_GetStatusStr(status), status);
        return ApplyResult::Failed;
    }

    bool currentlyPatched = false;
    const FunctionPatcherStatus queryStatus = FunctionPatcher_IsFunctionPatched(handle, &currentlyPatched);
    if (queryStatus == FUNCTION_PATCHER_RESULT_SUCCESS) {
        Logger::Info("Native hook %s REGISTERED: %s",
                     id.c_str(), currentlyPatched ? "ACTIVE" : "WAITING");
    } else {
        Logger::Warn("Native hook %s registered but state query failed: %s (%d)",
                     id.c_str(), FunctionPatcher_GetStatusStr(queryStatus), queryStatus);
    }

    if (outHandle != nullptr) {
        *outHandle = handle;
    }
    if (outInitiallyPatched != nullptr) {
        *outInitiallyPatched = initiallyPatched;
    }

    return ApplyResult::Applied;
}

void ClearRegistrations() {
    gEntries.clear();
}

} // namespace Solar::NativeHookRegistry
