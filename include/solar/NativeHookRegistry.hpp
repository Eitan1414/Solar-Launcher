#pragma once

#include <function_patcher/function_patching.h>
#include <string>

namespace Solar::NativeHookRegistry {

enum class ApplyResult {
    Applied,
    NotRegistered,
    Failed,
};

bool Register(const std::string &id, const function_replacement_data_t &data);
bool Contains(const std::string &id);
ApplyResult Apply(const std::string &id, PatchedFunctionHandle *outHandle, bool *outInitiallyPatched);
void ClearRegistrations();

} // namespace Solar::NativeHookRegistry
