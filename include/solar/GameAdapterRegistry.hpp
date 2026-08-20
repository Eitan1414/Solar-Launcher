#pragma once

#include <cstdint>

namespace Solar::GameAdapterRegistry {

// Clears registrations from the previous title, then registers any built-in
// adapter matching the current game. Returns true when an adapter matched.
bool PrepareForTitle(uint64_t titleId);

// Must be called only after PatchEngine::Clear() has removed applied hooks.
void Reset();

} // namespace Solar::GameAdapterRegistry
