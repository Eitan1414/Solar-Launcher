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

constexpr int ItemsPerPage = 8;
constexpr int MinPriority = -1000;
constexpr int MaxPriority = 1000;
constexpr int PriorityStep = 10;

constexpr uint32_t SolarOrange = 0xF5A000FF;
constexpr uint32_t ScreenBlack = 0x00000000;
constexpr int FontPixelWidth = 8;
constexpr int FontPixelHeight = 16;
constexpr int PanelDividerColumn = 41;
constexpr int HeaderBottomRow = 10;
constexpr int ContentBottomRow = 21;

constexpr const char *BinaryLogo[] = {
    "       1  |  0",
    "    0    \\|/    1",
    "  10  .-------.  01",
    "-----/ 0011000 \\-----",
    "    | 0011000 |",
    "1---| 0011110 |---0",
    "    | 0000110 |",
    "-----\\ 0000100 /-----",
    "  01  '-------'  10",
    "    1    /|\\    0",
};
constexpr size_t BinaryLogoLineCount = sizeof(BinaryLogo) / sizeof(BinaryLogo[0]);

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

    OSScreenClearBufferEx(SCREEN_TV, ScreenBlack);
    OSScreenClearBufferEx(SCREEN_DRC, ScreenBlack);
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
    char line[128] = {};

    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    line[79] = '\0';
    OSScreenPutFontEx(SCREEN_TV, x, y, line);
    OSScreenPutFontEx(SCREEN_DRC, x, y, line);
}

void DrawHorizontalLine(OSScreenID screen, int y, int width, uint32_t colour) {
    for (int x = 0; x < width; ++x) {
        OSScreenPutPixelEx(screen, static_cast<uint32_t>(x), static_cast<uint32_t>(y), colour);
        OSScreenPutPixelEx(screen, static_cast<uint32_t>(x), static_cast<uint32_t>(y + 1), colour);
    }
}

void DrawVerticalLine(OSScreenID screen, int x, int yStart, int yEnd, uint32_t colour) {
    for (int y = yStart; y < yEnd; ++y) {
        OSScreenPutPixelEx(screen, static_cast<uint32_t>(x), static_cast<uint32_t>(y), colour);
        OSScreenPutPixelEx(screen, static_cast<uint32_t>(x + 1), static_cast<uint32_t>(y), colour);
    }
}

void DrawSolarChrome() {
    constexpr int TvWidth = 1280;
    constexpr int DrcWidth = 854;
    const int headerY = HeaderBottomRow * FontPixelHeight;
    const int contentBottomY = ContentBottomRow * FontPixelHeight;
    const int dividerX = PanelDividerColumn * FontPixelWidth;
    const int contentStartY = (HeaderBottomRow + 1) * FontPixelHeight;

    DrawHorizontalLine(SCREEN_TV, headerY, TvWidth, SolarOrange);
    DrawHorizontalLine(SCREEN_DRC, headerY, DrcWidth, SolarOrange);
    DrawHorizontalLine(SCREEN_TV, contentBottomY, TvWidth, SolarOrange);
    DrawHorizontalLine(SCREEN_DRC, contentBottomY, DrcWidth, SolarOrange);

    DrawVerticalLine(SCREEN_TV, dividerX, contentStartY, contentBottomY, SolarOrange);
    DrawVerticalLine(SCREEN_DRC, dividerX, contentStartY, contentBottomY, SolarOrange);
}

void RenderHeader(size_t visibleLogoLines = BinaryLogoLineCount) {
    const size_t count = std::min(visibleLogoLines, BinaryLogoLineCount);
    for (size_t row = 0; row < count; ++row) {
        PrintBoth(1, static_cast<int>(row), "%s", BinaryLogo[row]);
    }

    if (visibleLogoLines >= 3) {
        PrintBoth(29, 2, "SOLAR LAUNCHER");
        PrintBoth(29, 3, "Universal Wii U modding framework");
    }
    if (visibleLogoLines >= 6) {
        PrintBoth(29, 5, "Load. Combine. Expand.");
        PrintBoth(29, 6, "v0.5 - Game Adapter build");
    }
}

void RenderBootAnimation() {
    for (size_t visible = 1; visible <= BinaryLogoLineCount; ++visible) {
        OSScreenClearBufferEx(SCREEN_TV, ScreenBlack);
        OSScreenClearBufferEx(SCREEN_DRC, ScreenBlack);
        RenderHeader(visible);

        const int headerY = HeaderBottomRow * FontPixelHeight;
        DrawHorizontalLine(SCREEN_TV, headerY, 1280, SolarOrange);
        DrawHorizontalLine(SCREEN_DRC, headerY, 854, SolarOrange);

        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
        OSSleepTicks(OSMillisecondsToTicks(35));
    }

    OSSleepTicks(OSMillisecondsToTicks(100));
}

void RenderDetails(uint64_t titleId,
                   const ModInfo &mod,
                   size_t conflictCount,
                   bool technicalDetails) {
    if (!technicalDetails) {
        PrintBoth(43, 11, "MOD INFORMATION");
        PrintBoth(43, 12, "Name:     %-30.30s", mod.name.c_str());
        PrintBoth(43, 13, "Author:   %-30.30s", mod.author.c_str());
        PrintBoth(43, 14, "Version:  %-30.30s", mod.version.c_str());
        PrintBoth(43, 15, "Type:     %-30.30s", mod.type.c_str());
        PrintBoth(43, 16, "Priority: %d", mod.priority);
        PrintBoth(43, 17, "Conflicts: %u", static_cast<unsigned int>(conflictCount));
        PrintBoth(43, 18, "Payload: C:%s A:%s P:%s",
                  mod.hasContent ? "yes" : "no",
                  mod.hasAoc ? "yes" : "no",
                  mod.hasPatches ? "yes" : "no");
        PrintBoth(43, 19, "Source:   %s", mod.legacySDCafiine ? "SDCafiine" : "Solar");
        PrintBoth(43, 20, "X: technical details");
        return;
    }

    PrintBoth(43, 11, "TECHNICAL DETAILS");
    PrintBoth(43, 12, "Title:  %s", TitleManager::FormatTitleId(titleId).c_str());
    PrintBoth(43, 13, "Folder: %-30.30s", mod.directoryName.c_str());
    PrintBoth(43, 14, "Source: %s", mod.legacySDCafiine ? "SDCafiine legacy pack" : "Solar mod");
    PrintBoth(43, 15, "Current: %s  P:%d", mod.enabled ? "enabled" : "disabled", mod.priority);
    PrintBoth(43, 16, "Default: %s  P:%d", mod.defaultEnabled ? "enabled" : "disabled", mod.defaultPriority);
    PrintBoth(43, 17, "content/: %s", mod.hasContent ? "present" : "none");
    PrintBoth(43, 18, "aoc/:     %s", mod.hasAoc ? "present" : "none");
    PrintBoth(43, 19, "patches/: %s", mod.hasPatches ? "present" : "none");
    PrintBoth(43, 20, "X: mod information");
}

void Render(uint64_t titleId,
            const std::vector<ModInfo> &mods,
            size_t selected,
            const ConflictReport &conflicts,
            bool technicalDetails) {
    OSScreenClearBufferEx(SCREEN_TV, ScreenBlack);
    OSScreenClearBufferEx(SCREEN_DRC, ScreenBlack);

    RenderHeader();
    DrawSolarChrome();

    const size_t page = selected / ItemsPerPage;
    const size_t start = page * ItemsPerPage;
    const size_t end = std::min(start + static_cast<size_t>(ItemsPerPage), mods.size());
    const size_t pages = std::max<size_t>(1, (mods.size() + ItemsPerPage - 1) / ItemsPerPage);

    PrintBoth(1, 11, "MODS  %u installed", static_cast<unsigned int>(mods.size()));

    int row = 12;
    for (size_t index = start; index < end; ++index, ++row) {
        const ModInfo &mod = mods[index];
        const size_t conflictCount = index < conflicts.perMod.size() ? conflicts.perMod[index] : 0;

        PrintBoth(1, row, "%c [%s] %-25.25s",
                  index == selected ? '>' : ' ',
                  mod.enabled ? "ON " : "OFF",
                  mod.name.c_str());

        if (conflictCount > 0) {
            PrintBoth(34, row, "!%u", static_cast<unsigned int>(conflictCount));
        } else if (mod.legacySDCafiine) {
            PrintBoth(34, row, "SDC");
        }
    }

    if (!mods.empty()) {
        const size_t conflictCount = selected < conflicts.perMod.size() ? conflicts.perMod[selected] : 0;
        RenderDetails(titleId, mods[selected], conflictCount, technicalDetails);
    }

    PrintBoth(1, 22, "A Toggle   X Details   L/R Priority   Y Reset");
    PrintBoth(1, 23, "+ Save & launch mods   B Launch vanilla once");
    PrintBoth(1, 24, "0101 SOLAR READY 1010   Page %u/%u   File conflicts: %u%s",
              static_cast<unsigned int>(page + 1),
              static_cast<unsigned int>(pages),
              static_cast<unsigned int>(conflicts.conflictingPaths),
              conflicts.truncated ? "+" : "");

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

    RenderBootAnimation();

    KPADInit();
    WPADEnableURCC(true);

    size_t selected = 0;
    bool changed = false;
    bool technicalDetails = false;
    ConflictReport conflicts = ConflictDetector::Analyze(mods);

    while (true) {
        Render(titleId, mods, selected, conflicts, technicalDetails);
        const uint32_t buttons = PollButtons();

        if (buttons & VPAD_BUTTON_UP) {
            selected = selected == 0 ? mods.size() - 1 : selected - 1;
        } else if (buttons & VPAD_BUTTON_DOWN) {
            selected = (selected + 1) % mods.size();
        } else if (buttons & VPAD_BUTTON_A) {
            mods[selected].enabled = !mods[selected].enabled;
            changed = true;
            conflicts = ConflictDetector::Analyze(mods);
        } else if (buttons & VPAD_BUTTON_X) {
            technicalDetails = !technicalDetails;
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
