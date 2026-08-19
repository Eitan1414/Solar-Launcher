#pragma once

#include <cstdint>
#include <string>

namespace Solar::TitleManager {

uint64_t CurrentTitleId();
std::string FormatTitleId(uint64_t titleId);
bool IsGameTitle(uint64_t titleId);

} // namespace Solar::TitleManager
