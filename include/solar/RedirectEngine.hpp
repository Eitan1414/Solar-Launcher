#pragma once

#include "solar/ModManager.hpp"

#include <cstddef>
#include <vector>

namespace Solar::RedirectEngine {

bool Initialize();
void Shutdown();
bool IsAvailable();
void Clear();
size_t Apply(const std::vector<ModInfo> &mods);

} // namespace Solar::RedirectEngine
