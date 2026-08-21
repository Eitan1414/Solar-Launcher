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

bool QueryPatchState(PatchedFunctionHandle handle,
                     const std::string &id,
                     const char *label,
                     bool &outPatched) {
    outPatched = false;
    const FunctionPatcherStatus queryStatus = FunctionPatcher_IsFunctionPatched(handle, &outPatched);
    if (queryStatus == FUNCTION_PATCHER_RESULT_SUCCESS) {
        Logger::Info("Native hook %s %s: %s",
                     id.c_str(),
                     label,
                     outPatched ? "ACTIVE" : "WAITING");
        return true;
    }

    Logger::Warn("Native hook %s %s state query failed: %s (%d)",
                 id.c_str(),
                 label,
                 FunctionPatcher_GetStatusStr(queryStatus),
                 queryStatus);
    return false;
}

bool RestoreExecutableRegistration(const std::string &id,
                                   function_replacement_data_t &data,
                                   PatchedFunctionHandle &outHandle,
                                   bool &outInitiallyPatched) {
    outHandle = {};
    outInitiallyPatched = false;

    const FunctionPatcherStatus restoreStatus = FunctionPatcher_AddFunctionPatch(
        &data, &outHandle, &outInitiallyPatched);
    if (restoreStatus != FUNCTION_PATCHER_RESULT_SUCCESS) {
        Logger::Error("Native hook %s could not restore executable-offset registration: %s (%d)",
                      id.c_str(), FunctionPatcher_GetStatusStr(restoreStatus), restoreStatus);
        return false;
    }

    bool restoredPatched = false;
    QueryPatchState(outHandle, id, "RESTORED EXECUTABLE-OFFSET", restoredPatched);
    return true;
}

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
    const bool querySucceeded = QueryPatchState(handle, id, "EXECUTABLE-OFFSET", currentlyPatched);

    // Test 1B safety net: MonoBridge stores a signature-verified effective and
    // physical address in these normally-unused fields. If FunctionPatcher has
    // accepted the executable+offset registration but it is still not active
    // even though the RPX is already loaded, remove that registration and retry
    // once with the verified direct address. We never keep both patches at once.
    const bool directFallbackArmed =
        data.type == FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_ADDRESS &&
        data.virtualAddr != 0 && data.physicalAddr != 0;

    if (querySucceeded && !currentlyPatched && directFallbackArmed) {
        Logger::Warn("Native hook %s executable-offset patch is WAITING; trying VERIFIED DIRECT fallback eff=%08X phys=%08X",
                     id.c_str(),
                     static_cast<unsigned int>(data.virtualAddr),
                     static_cast<unsigned int>(data.physicalAddr));

        const FunctionPatcherStatus removeStatus = FunctionPatcher_RemoveFunctionPatch(handle);
        if (removeStatus == FUNCTION_PATCHER_RESULT_SUCCESS) {
            function_replacement_data_t fallback = data;
            fallback.type = FUNCTION_PATCHER_REPLACE_BY_LIB_OR_ADDRESS;
            fallback.ReplaceInRPL.function_name = "solar.verified.direct";
            fallback.ReplaceInRPL.library = LIBRARY_OTHER;

            PatchedFunctionHandle fallbackHandle {};
            bool fallbackInitiallyPatched = false;
            const FunctionPatcherStatus fallbackStatus = FunctionPatcher_AddFunctionPatch(
                &fallback, &fallbackHandle, &fallbackInitiallyPatched);

            if (fallbackStatus == FUNCTION_PATCHER_RESULT_SUCCESS) {
                bool fallbackCurrentlyPatched = false;
                const bool fallbackQuerySucceeded = QueryPatchState(
                    fallbackHandle, id, "VERIFIED DIRECT FALLBACK", fallbackCurrentlyPatched);

                if (fallbackQuerySucceeded && fallbackCurrentlyPatched) {
                    Logger::Info("Native hook %s Test1B fallback SUCCESS", id.c_str());
                    handle = fallbackHandle;
                    initiallyPatched = fallbackInitiallyPatched;
                    currentlyPatched = true;
                } else {
                    Logger::Warn("Native hook %s direct fallback did not become ACTIVE; removing fallback and restoring executable registration",
                                 id.c_str());
                    const FunctionPatcherStatus fallbackRemoveStatus = FunctionPatcher_RemoveFunctionPatch(fallbackHandle);
                    if (fallbackRemoveStatus != FUNCTION_PATCHER_RESULT_SUCCESS) {
                        Logger::Warn("Native hook %s failed to remove inactive direct fallback: %s (%d)",
                                     id.c_str(),
                                     FunctionPatcher_GetStatusStr(fallbackRemoveStatus),
                                     fallbackRemoveStatus);
                    }

                    if (!RestoreExecutableRegistration(id, data, handle, initiallyPatched)) {
                        return ApplyResult::Failed;
                    }
                }
            } else {
                Logger::Warn("Native hook %s direct fallback registration failed: %s (%d); restoring executable registration",
                             id.c_str(), FunctionPatcher_GetStatusStr(fallbackStatus), fallbackStatus);
                if (!RestoreExecutableRegistration(id, data, handle, initiallyPatched)) {
                    return ApplyResult::Failed;
                }
            }
        } else {
            Logger::Warn("Native hook %s could not remove WAITING executable registration before fallback: %s (%d)",
                         id.c_str(), FunctionPatcher_GetStatusStr(removeStatus), removeStatus);
        }
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
