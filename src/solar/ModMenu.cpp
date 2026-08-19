#include "solar/ModMenu.hpp"

#include "solar/ConflictDetector.hpp"
#include "solar/Logger.hpp"
#include "solar/SelectionStore.hpp"
#include "solar/TitleManager.hpp"

#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <memory/mappedmemory.h>
#include <padscore/kpad.h>
#include <vpad/input.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace Solar {
namespace {

constexpr int ItemsPerPage = 10;
constexpr int MinPriority = -1000;
constexpr int MaxPriority = 1000;
constexpr int PriorityStep = 10;

void *gTvBuffer = nullptr;
void *gDrcBuffer = nullptr;

uint32_t RemapWiiRemoteButtons(uint32_t buttons) {
    uint32_t out = 0;

    if (buttons & WPAD_BUTTON_LEFT) out |= VPAD_BUTTON_LEFT;
    if (buttons & WPAD_BUTTON_RIGHT) out |= VPAD_BUTTON_RIGHT;
    if (buttons & WPAD_BUTTON_DOWN) out |= VPAD_BUTTON_DOWN;
    if (buttons & WPAD_BUTTON_UP) out |= VPAD_BUTTON_UP;
    if (buttons & WPAD_BUTTON_PLUS) out |= VPAD_BUTTON_PLUS;
    if (buttons & WPAD_BUTTON_MINUS) out |= VPAD_BUTTON_MINUS;
    if (buttons & WPAD_BUTTON_A) out |= VPAD_BUTTON_A;
    if (buttons & WPAD_BUTTON_B) out |= VPAD_BUTTON_B;

    return out;
}

uint32_t RemapClassicButtons(uint32_t buttons) {
    uint32_t out = 0;

    if (buttons & WPAD_CLASSIC_BUTTON_LEFT) out |= VPAD_BUTTON_LEFT;
    if (buttons & WPAD_CLASSIC_BUTTON_RIGHT) out |= VPAD_BUTTON_RIGHT;
    if (buttons & WPAD_CLASSIC_BUTTON_DOWN) out |= VPAD_BUTTON_DOWN;
    if (buttons & WPAD_CLASSIC_BUTTON_UP) out |= VPAD_BUTTON_UP;
    if (buttons & WPAD_CLASSIC_BUTTON_PLUS) out |= VPAD_BUTTON_PLUS;
    if (buttons & WPAD_CLASSIC_BUTTON_MINUS) out |= VPAD_BUTTON_MINUS;
    if (buttons & WPAD_CLASSIC_BUTTON_A) out |= VPAD_BUTTON_A;
    if (buttons & WPAD_CLASSIC_BUTTON_B) out |= VPAD_BUTTON_B;
    if (buttons & WPAD_CLASSIC_BUTTON_X) out |= VPAD_BUTTON_X;
    if (buttons & WPAD_CLASSIC_BUTTON_Y) out |= VPAD_BUTTON_Y;
    if (buttons & WPAD_CLASSIC_BUTTON_L) out |= VPAD_BUTTON_L;
    if (buttons & WPAD_CLASSIC_BUTTON_R) out |= VPAD_BUTTON_R;

    return out;
}

bool InitScreen() {
    OSScreenInit();

    const uint32_t tvSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    const uint32_t drcSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    gTvBuffer = MEMAllocFromMappedMemoryForGX2Ex(tvSize, 0x100);
    gDrcBuffer = MEMAllocFromMappedMemoryForGX2Ex(drcSize, 0x100);

    if (gTvBuffer == nullptr || gDrcBuffer == nullptr) {
        if (gTvBuffer != nullptr) {
            MEMFreeToMappedMemory(gTvBuffer);
            gTvBuffer = nullptr;
        }
        if (gDrcBuffer != nullptr) {
            MEMFreeToMappedMemory(gDrcBuffer);
            gDrcBuffer = nullptr;
        }
        return false;
    }

    OSScreenSetBufferEx(SCREEN_TV, gTvBuffer);
    OSScreenSetBufferEx(SCREEN_DRC, gDrcBuffer);
    OSScreenEnableEx(SCREEN_TV, true);
    OSScreenEnableEx(SCREEN_DRC, true);
    return true;
}

void DeinitScreen() {
    if (gTvBuffer == nullptr && gDrcBuffer == nullptr) {
        return;
    }

    OSScreenClearBufferEx(SCREEN_TV, 0);
    OSScreenClearBufferEx(SCREEN_DRC, 0);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);

    if (gTvBuffer != nullptr) {
        MEMFreeToMappedMemory(gTvBuffer);
        gTvBuffer = nullptr;
    }
    if (gDrcBuffer != nullptr) {
        MEMFreeToMappedMemory(gDrcBuffer);
        gDrcBuffer = nullptr;
    }
}

void PrintBoth(int x, int y, const char *format, ...) {
    char line[128];

    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    line[79] = '\0';
    OSScreenPutFontEx(SCREEN_TV, x, y, line);
    OSScreenPutFontEx(SCREEN_DRC, x, y, line);
}

void Render(uint64_t titleId,
            const std::vector<ModInfo> &mods,
            size_t selected,
            const ConflictReport &conflicts) {
    OSScreenClearBufferEx(SCREEN_TV, 0);
    OSScreenClearBufferEx(SCREEN_DRC, 0);

    PrintBoth(0, 0, "Solar Launcher v0.3");
    PrintBoth(0, 1, "Title: %s", TitleManager::FormatTitleId(titleId).c_str());
    PrintBoth(0, 2, "Mods: %u | Conflicts: %u%s",
              static_cast<unsigned int>(mods.size()),
              static_cast<unsigned int>(conflicts.conflictingPaths),
              conflicts.truncated ? "+" : "");

    const size_t page = selected / ItemsPerPage;
    const size_t start = page * ItemsPerPage;
    const size_t end = std::min(start + static_cast<size_t>(ItemsPerPage), mods.size());

    int row = 4;
    for (size_t index = start; index < end; ++index, ++row) {
        const ModInfo &mod = mods[index];
        const size_t conflictCount = index < conflicts.perMod.size() ? conflicts.perMod[index] : 0;

        char source[8] = "";
        if (mod.legacySDCafiine) {
            strcpy(source, " SDC");
        }

        PrintBoth(0, row, "%c [%c] %-30.30s P:%4d C:%u%s",
                  index == selected ? '>' : ' ',
                  mod.enabled ? 'X' : ' ',
                  mod.name.c_str(),
                  mod.priority,
                  static_cast<unsigned int>(conflictCount),
                  source);
    }

    if (!mods.empty()) {
        const ModInfo &mod = mods[selected];
        PrintBoth(0, 15, "%s | %s | %s",
                  mod.author.c_str(), mod.version.c_str(), mod.type.c_str());
        PrintBoth(0, 16, "Files: content=%s aoc=%s",
                  mod.hasContent ? "yes" : "no",
                  mod.hasAoc ? "yes" : "no");
    }

    PrintBoth(0, 18, "A Toggle   L/R Priority   Y Reset selected");
    PrintBoth(0, 19, "+ Save & launch mods   B Launch vanilla once");

    if (mods.size() > ItemsPerPage) {
        const size_t pages = (mods.size() + ItemsPerPage - 1) / ItemsPerPage;
        PrintBoth(0, 20, "Page %u/%u",
                  static_cast<unsigned int>(page + 1),
                  static_cast<unsigned int>(pages));
    }

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

uint32_t PollButtons() {
    uint32_t triggered = 0;

    VPADStatus vpad {};
    VPADReadError vpadError;
    VPADRead(VPAD_CHAN_0, &vpad, 1, &vpadError);
    if (vpadError == VPAD_READ_SUCCESS) {
        triggered |= vpad.trigger;
    }

    for (int channel = 0; channel < 4; ++channel) {
        KPADStatus status {};
        KPADError error;

        if (KPADReadEx(static_cast<KPADChan>(channel), &status, 1, &error) <= 0 ||
            error != KPAD_ERROR_OK || status.extensionType == 0xFF) {
            continue;
        }

        if (status.extensionType == WPAD_EXT_CORE || status.extensionType == WPAD_EXT_NUNCHUK) {
            triggered |= RemapWiiRemoteButtons(status.trigger);
        } else {
            triggered |= RemapClassicButtons(status.classic.trigger);
        }
    }

    return triggered;
}

} // namespace

MenuResult ModMenu::Show(uint64_t titleId, std::vector<ModInfo> &mods) {
    if (mods.empty()) {
        return {MenuAction::ApplySelected, false};
    }

    if (!InitScreen()) {
        Logger::Error("Failed to initialize Solar pre-launch screen");
        return {MenuAction::Failed, false};
    }

    KPADInit();
    WPADEnableURCC(true);

    size_t selected = 0;
    bool changed = false;
    ConflictReport conflicts = ConflictDetector::Analyze(mods);

    while (true) {
        Render(titleId, mods, selected, conflicts);
        const uint32_t buttons = PollButtons();

        if (buttons & VPAD_BUTTON_UP) {
            selected = selected == 0 ? mods.size() - 1 : selected - 1;
        } else if (buttons & VPAD_BUTTON_DOWN) {
            selected = (selected + 1) % mods.size();
        } else if (buttons & VPAD_BUTTON_A) {
            mods[selected].enabled = !mods[selected].enabled;
            changed = true;
            conflicts = ConflictDetector::Analyze(mods);
        } else if (buttons & VPAD_BUTTON_L) {
            mods[selected].priority = std::max(MinPriority, mods[selected].priority - PriorityStep);
            changed = true;
        } else if (buttons & VPAD_BUTTON_R) {
            mods[selected].priority = std::min(MaxPriority, mods[selected].priority + PriorityStep);
            changed = true;
        } else if (buttons & VPAD_BUTTON_Y) {
            mods[selected].enabled = mods[selected].defaultEnabled;
            mods[selected].priority = mods[selected].defaultPriority;
            changed = true;
            conflicts = ConflictDetector::Analyze(mods);
        } else if (buttons & VPAD_BUTTON_PLUS) {
            const bool saved = SelectionStore::Save(titleId, mods);
            if (!saved) {
                Logger::Warn("Selection could not be saved; applying it for this launch only");
            }
            KPADShutdown();
            DeinitScreen();
            return {MenuAction::ApplySelected, changed};
        } else if (buttons & VPAD_BUTTON_B) {
            KPADShutdown();
            DeinitScreen();
            return {MenuAction::LaunchVanilla, changed};
        }

        OSSleepTicks(OSMillisecondsToTicks(16));
    }
}

} // namespace Solar
