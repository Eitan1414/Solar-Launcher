#pragma once

#include "solar/ModManager.hpp"

#include <cstdint>
#include <vector>

namespace Solar::SelectionStore {

bool Load(uint64_t titleId, std::vector<ModInfo> &mods);
bool Save(uint64_t titleId, const std::vector<ModInfo> &mods);

} // namespace Solar::SelectionStore
