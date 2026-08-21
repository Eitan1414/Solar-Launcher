#include "solar/ModMenu.hpp"

#include "solar/ConflictDetector.hpp"
#include "solar/EmbeddedLogo.hpp"
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

constexpr int ItemsPerPage = 7;
constexpr int MinPriority = -1000;
constexpr int MaxPriority = 1000;
constexpr int PriorityStep = 10;

constexpr uint32_t SolarOrange = 0xF5A000FF;
constexpr uint32_t ScreenBlack = 0x00000000;
constexpr uint32_t ScreenWhite = 0xFFFFFFFF;
constexpr int TvWidth = 1280;
constexpr int DrcWidth = 854;
constexpr int TvTextColumns = 158;
constexpr int DrcTextColumns = 106;
constexpr int HeaderSeparatorY = 238;
constexpr int FooterSeparatorY = 374;
constexpr int ContentHeaderRow = 15;
constexpr int ContentFirstRow = 16;
constexpr int FooterFirstRow = 24;

// Keep the reference artwork intact, but use a smaller independent scale on DRC.
constexpr int TvLogoWidth = 900;
constexpr int TvLogoHeight = (TvLogoWidth * EmbeddedLogo::Height) / EmbeddedLogo::Width;
constexpr int TvLogoY = 0;
constexpr int DrcLogoWidth = 680;
constexpr int DrcLogoHeight = (DrcLogoWidth * EmbeddedLogo::Height) / EmbeddedLogo::Width;
constexpr int DrcLogoY = 26;

void *gTvBuffer = nullptr;
void *gDrcBuffer = nullptr;
uint8_t gLogoBitmap[EmbeddedLogo::BitmapSize] = {};
bool gLogoDecodeAttempted = false;
bool gLogoReady = false;

int Base64Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

size_t DecodeBase64(const char *input, uint8_t *output, size_t outputCapacity) {
    if (input == nullptr || output == nullptr || outputCapacity == 0) {
        return 0;
    }

    uint32_t accumulator = 0;
    int bits = 0;
    size_t written = 0;

    for (const char *cursor = input; *cursor != '\0'; ++cursor) {
        if (*cursor == '=') {
            break;
        }

        const int value = Base64Value(*cursor);
        if (value < 0) {
            continue;
        }

        accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            if (written >= outputCapacity) {
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
    const size_t compressedSize = DecodeBase64(EmbeddedLogo::Base64Zlib,
                                               compressed,
                                               sizeof(compressed));
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
                 EmbeddedLogo::Width,
                 EmbeddedLogo::Height);
    return true;
}

bool LogoPixel(int x, int y) {
    if (!gLogoReady || x < 0 || y < 0 ||
        x >= EmbeddedLogo::Width || y >= EmbeddedLogo::Height) {
        return false;
    }

    const uint8_t value = gLogoBitmap[y * EmbeddedLogo::RowBytes + (x >> 3)];
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
        const int sy = (dy * EmbeddedLogo::Height) / targetHeight;
        const int py = yOrigin + dy;

        for (int dx = 0; dx < targetWidth; ++dx) {
            const int sx = (dx * EmbeddedLogo::Width) / targetWidth;
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

void DrawSolarChrome() {
    // Only two separators remain: one under the logo and one above controls.
    DrawHorizontalLine(SCREEN_TV, HeaderSeparatorY, TvWidth, SolarOrange);
    DrawHorizontalLine(SCREEN_DRC, HeaderSeparatorY, DrcWidth, SolarOrange);
    DrawHorizontalLine(SCREEN_TV, FooterSeparatorY, TvWidth, SolarOrange);
    DrawHorizontalLine(SCREEN_DRC, FooterSeparatorY, DrcWidth, SolarOrange);
}

void RenderHeader() {
    if (!EnsureLogoBitmap()) {
        PrintBoth(2, 3, "SOLAR LAUNCHER");
        PrintBoth(2, 4, "Universal Wii U modding framework");
        return;
    }

    DrawLogoBitmap(SCREEN_TV, TvWidth, TvLogoWidth, TvLogoHeight, TvLogoY);
    DrawLogoBitmap(SCREEN_DRC, DrcWidth, DrcLogoWidth, DrcLogoHeight, DrcLogoY);
}

void RenderBootAnimation() {
    OSScreenClearBufferEx(SCREEN_TV, ScreenBlack);
    OSScreenClearBufferEx(SCREEN_DRC, ScreenBlack);
    RenderHeader();

    DrawHorizontalLine(SCREEN_TV, HeaderSeparatorY, TvWidth, SolarOrange);
    DrawHorizontalLine(SCREEN_DRC, HeaderSeparatorY, DrcWidth, SolarOrange);

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
    OSSleepTicks(OSMillisecondsToTicks(180));
}

void RenderDetails(uint64_t titleId,
                   const ModInfo &mod,
                   size_t conflictCount,
                   bool technicalDetails) {
    if (!technicalDetails) {
        PrintBoth(43, ContentHeaderRow, "MOD INFORMATION");
        PrintBoth(43, ContentFirstRow + 0, "Name:    %-31.31s", mod.name.c_str());
        PrintBoth(43, ContentFirstRow + 1, "Author:  %-23.23s  Ver: %.12s", mod.author.c_str(), mod.version.c_str());
        PrintBoth(43, ContentFirstRow + 2, "Type: %-10.10s  Priority: %d", mod.type.c_str(), mod.priority);
        PrintBoth(43, ContentFirstRow + 3, "Conflicts: %u  Payload C:%s A:%s P:%s",
                  static_cast<unsigned int>(conflictCount),
                  mod.hasContent ? "yes" : "no",
                  mod.hasAoc ? "yes" : "no",
                  mod.hasPatches ? "yes" : "no");
        PrintBoth(43, ContentFirstRow + 4, "Source: %s", mod.legacySDCafiine ? "SDCafiine" : "Solar");
        PrintBoth(43, ContentFirstRow + 5, "X: technical details");
        return;
    }

    PrintBoth(43, ContentHeaderRow, "TECHNICAL DETAILS");
    PrintBoth(43, ContentFirstRow + 0, "Title:  %s", TitleManager::FormatTitleId(titleId).c_str());
    PrintBoth(43, ContentFirstRow + 1, "Folder: %-30.30s", mod.directoryName.c_str());
    PrintBoth(43, ContentFirstRow + 2, "Source: %s", mod.legacySDCafiine ? "SDCafiine legacy pack" : "Solar mod");
    PrintBoth(43, ContentFirstRow + 3, "Current: %s P:%d | Default: %s P:%d",
              mod.enabled ? "ON" : "OFF", mod.priority,
              mod.defaultEnabled ? "ON" : "OFF", mod.defaultPriority);
    PrintBoth(43, ContentFirstRow + 4, "content:%s  aoc:%s  patches:%s",
              mod.hasContent ? "yes" : "no",
              mod.hasAoc ? "yes" : "no",
              mod.hasPatches ? "yes" : "no");
    PrintBoth(43, ContentFirstRow + 5, "X: mod information");
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

    PrintBoth(1, ContentHeaderRow, "MODS  %u installed", static_cast<unsigned int>(mods.size()));

    int row = ContentFirstRow;
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

    PrintBoth(1, FooterFirstRow + 0, "A Toggle   X Details   L/R Priority   Y Reset");
    PrintBoth(1, FooterFirstRow + 1, "+ Save & launch mods   B Launch vanilla once");
    PrintBoth(1, FooterFirstRow + 2, "0101 SOLAR READY 1010 | Test1B   Page %u/%u   File conflicts: %u%s",
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
