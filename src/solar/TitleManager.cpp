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

} // namespace Solar::TitleManager
