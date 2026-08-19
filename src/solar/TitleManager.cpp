#include "solar/TitleManager.hpp"

#include <coreinit/title.h>
#include <cstdio>

namespace Solar::TitleManager {

uint64_t CurrentTitleId() {
    return OSGetTitleID();
}

std::string FormatTitleId(uint64_t titleId) {
    char buffer[17] = {};
    snprintf(buffer, sizeof(buffer), "%016llX", static_cast<unsigned long long>(titleId));
    return buffer;
}

bool IsGameTitle(uint64_t titleId) {
    // Standard Wii U application/game titles use 00050000 as the upper 32 bits.
    // Solar V0.1 intentionally ignores system titles such as the Wii U Menu.
    return static_cast<uint32_t>(titleId >> 32) == 0x00050000;
}

} // namespace Solar::TitleManager
