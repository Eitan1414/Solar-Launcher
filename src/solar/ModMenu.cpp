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
#include <zlib.h>

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
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
constexpr uint32_t ScreenWhite = 0xFFFFFFFF;
constexpr int FontPixelWidth = 8;
constexpr int FontPixelHeight = 16;
constexpr int PanelDividerColumn = 41;
constexpr int HeaderBottomRow = 16;
constexpr int ContentBottomRow = 26;
constexpr int TvTextColumns = 158;
constexpr int DrcTextColumns = 106;

constexpr int LogoSourceWidth = 790;
constexpr int LogoSourceHeight = 205;
constexpr int LogoRowBytes = 99;
constexpr size_t LogoBitmapSize = static_cast<size_t>(LogoRowBytes) * LogoSourceHeight;

// Monochrome 1-bit bitmap generated directly from the user's approved
// Solar Launcher ASCII/binary reference. The packed bitmap is zlib-compressed
// and base64-encoded so the exact artwork can live inside SolarLauncher.wps.
constexpr const char *EmbeddedLogoBase64 =
    "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";

void *gTvBuffer = nullptr;
void *gDrcBuffer = nullptr;
uint8_t gLogoBitmap[LogoBitmapSize] = {};
bool gLogoDecodeAttempted = false;
bool gLogoReady = false;

int Base64Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return 26 + (c - 'a');
    if (c >= '0' && c <= '9') return 52 + (c - '0');
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

size_t DecodeBase64(const char *input, uint8_t *output, size_t capacity) {
    if (input == nullptr || output == nullptr || capacity == 0) {
        return 0;
    }

    uint32_t accumulator = 0;
    int bits = 0;
    size_t written = 0;

    for (const char *p = input; *p != '\0' && *p != '='; ++p) {
        const int value = Base64Value(*p);
        if (value < 0) {
            continue;
        }

        accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            if (written >= capacity) {
                return 0;
            }
            output[written++] = static_cast<uint8_t>((accumulator >> bits) & 0xFFu);
            if (bits == 0) {
                accumulator = 0;
            } else {
                accumulator &= (1u << bits) - 1u;
            }
        }
    }

    return written;
}

bool EnsureLogoBitmap() {
    if (gLogoDecodeAttempted) {
        return gLogoReady;
    }
    gLogoDecodeAttempted = true;

    uint8_t compressed[4608] = {};
    const size_t compressedSize = DecodeBase64(EmbeddedLogoBase64, compressed, sizeof(compressed));
    if (compressedSize == 0) {
        Logger::Warn("Solar logo bitmap: base64 decode failed");
        return false;
    }

    uLongf outputSize = static_cast<uLongf>(sizeof(gLogoBitmap));
    const int zResult = uncompress(gLogoBitmap,
                                   &outputSize,
                                   compressed,
                                   static_cast<uLong>(compressedSize));
    if (zResult != Z_OK || outputSize != sizeof(gLogoBitmap)) {
        Logger::Warn("Solar logo bitmap: zlib decode failed (%d, %u/%u)",
                     zResult,
                     static_cast<unsigned int>(outputSize),
                     static_cast<unsigned int>(sizeof(gLogoBitmap)));
        return false;
    }

    gLogoReady = true;
    Logger::Info("Solar logo bitmap loaded from embedded reference (%dx%d)",
                 LogoSourceWidth,
                 LogoSourceHeight);
    return true;
}

bool LogoPixel(int x, int y) {
    if (!gLogoReady || x < 0 || y < 0 || x >= LogoSourceWidth || y >= LogoSourceHeight) {
        return false;
    }

    const uint8_t value = gLogoBitmap[y * LogoRowBytes + (x >> 3)];
    return (value & static_cast<uint8_t>(0x80u >> (x & 7))) != 0;
}

void DrawLogoBitmap(OSScreenID screen,
                    int screenWidth,
                    int targetWidth,
                    int targetHeight,
                    int yOrigin) {
    if (!EnsureLogoBitmap()) {
        return;
    }

    const int xOrigin = std::max(0, (screenWidth - targetWidth) / 2);

    for (int dy = 0; dy < targetHeight; ++dy) {
        const int sy = (dy * LogoSourceHeight) / targetHeight;
        const int py = yOrigin + dy;

        for (int dx = 0; dx < targetWidth; ++dx) {
            const int sx = (dx * LogoSourceWidth) / targetWidth;
            if (!LogoPixel(sx, sy)) {
                continue;
            }
            OSScreenPutPixelEx(screen,
                               static_cast<uint32_t>(xOrigin + dx),
                               static_cast<uint32_t>(py),
                               ScreenWhite);
        }
    }
}

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

void PutClippedLine(OSScreenID screen, int columns, int x, int y, const char *line) {
    if (line == nullptr || x < 0 || x >= columns) {
        return;
    }

    char clipped[192] = {};
    std::snprintf(clipped, sizeof(clipped), "%s", line);

    const int available = columns - x;
    if (available <= 0) {
        return;
    }
    if (available < static_cast<int>(sizeof(clipped))) {
        clipped[available] = '\0';
    }

    OSScreenPutFontEx(screen, x, y, clipped);
}

void PrintBoth(int x, int y, const char *format, ...) {
    char line[192] = {};

    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    PutClippedLine(SCREEN_TV, TvTextColumns, x, y, line);
    PutClippedLine(SCREEN_DRC, DrcTextColumns, x, y, line);
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

void RenderHeader() {
    if (!EnsureLogoBitmap()) {
        PrintBoth(2, 3, "SOLAR LAUNCHER");
        PrintBoth(2, 4, "Universal Wii U modding framework");
        return;
    }

    // TV gets a slightly larger copy; the GamePad keeps the original 790x205
    // bitmap scale. Neither version reconstructs or drops any ASCII characters.
    DrawLogoBitmap(SCREEN_TV, 1280, 980, 254, 0);
    DrawLogoBitmap(SCREEN_DRC, 854, 790, 205, 18);
}

void RenderBootAnimation() {
    OSScreenClearBufferEx(SCREEN_TV, ScreenBlack);
    OSScreenClearBufferEx(SCREEN_DRC, ScreenBlack);
    RenderHeader();

    const int headerY = HeaderBottomRow * FontPixelHeight;
    DrawHorizontalLine(SCREEN_TV, headerY, 1280, SolarOrange);
    DrawHorizontalLine(SCREEN_DRC, headerY, 854, SolarOrange);

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
    OSSleepTicks(OSMillisecondsToTicks(180));
}

void RenderDetails(uint64_t titleId,
                   const ModInfo &mod,
                   size_t conflictCount,
                   bool technicalDetails) {
    if (!technicalDetails) {
        PrintBoth(43, 17, "MOD INFORMATION");
        PrintBoth(43, 18, "Name:     %-30.30s", mod.name.c_str());
        PrintBoth(43, 19, "Author:   %-30.30s", mod.author.c_str());
        PrintBoth(43, 20, "Version:  %-30.30s", mod.version.c_str());
        PrintBoth(43, 21, "Type:     %-30.30s", mod.type.c_str());
        PrintBoth(43, 22, "Priority: %d", mod.priority);
        PrintBoth(43, 23, "Conflicts: %u", static_cast<unsigned int>(conflictCount));
        PrintBoth(43, 24, "Payload: C:%s A:%s P:%s",
                  mod.hasContent ? "yes" : "no",
                  mod.hasAoc ? "yes" : "no",
                  mod.hasPatches ? "yes" : "no");
        PrintBoth(43, 25, "Source:   %s", mod.legacySDCafiine ? "SDCafiine" : "Solar");
        PrintBoth(43, 26, "X: technical details");
        return;
    }

    PrintBoth(43, 17, "TECHNICAL DETAILS");
    PrintBoth(43, 18, "Title:  %s", TitleManager::FormatTitleId(titleId).c_str());
    PrintBoth(43, 19, "Folder: %-30.30s", mod.directoryName.c_str());
    PrintBoth(43, 20, "Source: %s", mod.legacySDCafiine ? "SDCafiine legacy pack" : "Solar mod");
    PrintBoth(43, 21, "Current: %s  P:%d", mod.enabled ? "enabled" : "disabled", mod.priority);
    PrintBoth(43, 22, "Default: %s  P:%d", mod.defaultEnabled ? "enabled" : "disabled", mod.defaultPriority);
    PrintBoth(43, 23, "content/: %s", mod.hasContent ? "present" : "none");
    PrintBoth(43, 24, "aoc/:     %s", mod.hasAoc ? "present" : "none");
    PrintBoth(43, 25, "patches/: %s", mod.hasPatches ? "present" : "none");
    PrintBoth(43, 26, "X: mod information");
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

    PrintBoth(1, 17, "MODS  %u installed", static_cast<unsigned int>(mods.size()));

    int row = 18;
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

    PrintBoth(1, 27, "A Toggle   X Details   L/R Priority   Y Reset");
    PrintBoth(1, 28, "+ Save & launch mods   B Launch vanilla once");
    PrintBoth(1, 29, "0101 SOLAR READY 1010 | Test1B   Page %u/%u   File conflicts: %u%s",
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
    bool dirty = true;
    ConflictReport conflicts = ConflictDetector::Analyze(mods);

    while (true) {
        if (dirty) {
            Render(titleId, mods, selected, conflicts, technicalDetails);
            dirty = false;
        }

        const uint32_t buttons = PollButtons();

        if (buttons & VPAD_BUTTON_UP) {
            selected = selected == 0 ? mods.size() - 1 : selected - 1;
            dirty = true;
        } else if (buttons & VPAD_BUTTON_DOWN) {
            selected = (selected + 1) % mods.size();
            dirty = true;
        } else if (buttons & VPAD_BUTTON_A) {
            mods[selected].enabled = !mods[selected].enabled;
            changed = true;
            conflicts = ConflictDetector::Analyze(mods);
            dirty = true;
        } else if (buttons & VPAD_BUTTON_X) {
            technicalDetails = !technicalDetails;
            dirty = true;
        } else if (buttons & VPAD_BUTTON_L) {
            mods[selected].priority = std::max(MinPriority, mods[selected].priority - PriorityStep);
            changed = true;
            dirty = true;
        } else if (buttons & VPAD_BUTTON_R) {
            mods[selected].priority = std::min(MaxPriority, mods[selected].priority + PriorityStep);
            changed = true;
            dirty = true;
        } else if (buttons & VPAD_BUTTON_Y) {
            mods[selected].enabled = mods[selected].defaultEnabled;
            mods[selected].priority = mods[selected].defaultPriority;
            changed = true;
            conflicts = ConflictDetector::Analyze(mods);
            dirty = true;
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
