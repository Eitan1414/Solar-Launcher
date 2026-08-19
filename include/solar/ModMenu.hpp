#pragma once

#include "solar/ModManager.hpp"

#include <cstdint>
#include <vector>

namespace Solar {

enum class MenuAction {
    ApplySelected,
    LaunchVanilla,
    Failed
};

struct MenuResult {
    MenuAction action = MenuAction::Failed;
    bool selectionChanged = false;
};

class ModMenu {
public:
    static MenuResult Show(uint64_t titleId, std::vector<ModInfo> &mods);
};

} // namespace Solar
