#pragma once

#include <cstdint>

namespace Solar::CupheadAdapter {

constexpr uint64_t TitleId = 0x0005000021000000ULL;
constexpr uint16_t SupportedVersion = 0;
constexpr int PlayerThreeId = 2;
// Test 1B hardware log shows the Unity RPX with this full loader module path.
// FunctionPatcher name lookups compare the kernel module name exactly, so this
// verification branch intentionally targets the path reported by the console.
constexpr const char *ExecutableName = "G:\\Git\\CupheadLibrary\\Builds\\salt\\_Intermediate\\Unity-master.rpx";
constexpr const char *MonoTraceHookId = "cuphead.mono.compileTrace";

bool Supports(uint64_t titleId);
bool RegisterHooks();

} // namespace Solar::CupheadAdapter
