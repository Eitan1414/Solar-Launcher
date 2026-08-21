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

constexpr const char *LauncherVersion = "v0.5.1-polish";
constexpr int ItemsPerPage = 4;
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

constexpr int TvLogoWidth = 900;
constexpr int TvLogoHeight = (TvLogoWidth * EmbeddedLogo::Height) / EmbeddedLogo::Width;
constexpr int TvHeaderSeparatorY = 238;
constexpr int TvFooterSeparatorY = 374;
constexpr int TvContentHeaderRow = 15;
constexpr int TvContentFirstRow = 16;
constexpr int TvInfoColumn = 43;
constexpr int TvFooterFirstRow = 24;

// GamePad uses its own spacious vertical layout. Four mods per page leaves
// enough room for details and controls without squeezing the text together.
constexpr int DrcLogoWidth = 500;
constexpr int DrcLogoHeight = (DrcLogoWidth * EmbeddedLogo::Height) / EmbeddedLogo::Width;
constexpr int DrcLogoY = 5;
constexpr int DrcHeaderSeparatorY = 150;
constexpr int DrcModsHeaderRow = 10;
constexpr int DrcModsFirstRow = 11;
constexpr int DrcInfoSeparatorY = 244;
constexpr int DrcInfoHeaderRow = 16;
constexpr int DrcInfoFirstRow = 17;
constexpr int DrcControlsSeparatorY = 365;
constexpr int DrcControlsFirstRow = 24;

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
    if (input == nullptr || output == nullptr || outputCapacity == 0) return 0;

    uint32_t accumulator = 0;
    int bits = 0;
    size_t written = 0;
    for (const char *cursor = input; *cursor != '\0'; ++cursor) {
        if (*cursor == '=') break;
        const int value = Base64Value(*cursor);
        if (value < 0) continue;

        accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (written >= outputCapacity) return 0;
            output[written++] = static_cast<uint8_t>((accumulator >> bits) & 0xFFu);
            if (bits == 0) accumulator = 0;
            else accumulator &= (1u << bits) - 1u;
        }
    }
    return written;
}

bool EnsureLogoBitmap() {
    if (gLogoDecodeAttempted) return gLogoReady;
    gLogoDecodeAttempted = true;

    uint8_t compressed[4608] = {};
    const size_t compressedSize = DecodeBase64(
        EmbeddedLogo::Base64Zlib, compressed, sizeof(compressed));
    if (compressedSize == 0) {
        Logger::Warn("Solar logo bitmap: base64 decode failed");
        return false;
    }

    uLongf outputSize = static_cast<uLongf>(sizeof(gLogoBitmap));
    const int zResult = uncompress(gLogoBitmap, &outputSize, compressed,
                                   static_cast<uLong>(compressedSize));
    if (zResult != Z_OK || outputSize != sizeof(gLogoBitmap)) {
        Logger::Warn("Solar logo bitmap: zlib decode failed (%d, %u/%u)",
                     zResult,
                     static_cast<unsigned int>(outputSize),
                     static_cast<unsigned int>(sizeof(gLogoBitmap)));
        return false;
    }

    gLogoReady = true;
    return true;
}

bool LogoPixel(int x, int y) {
    if (!gLogoReady || x < 0 || y < 0 ||
        x >= EmbeddedLogo::Width || y >= EmbeddedLogo::Height) return false;
    const uint8_t value = gLogoBitmap[y * EmbeddedLogo::RowBytes + (x >> 3)];
    return (value & static_cast<uint8_t>(0x80u >> (x & 7))) != 0;
}

void DrawLogo(OSScreenID screen, int width, int targetWidth, int targetHeight, int yOrigin) {
    if (!EnsureLogoBitmap()) return;
    const int xOrigin = std::max(0, (width - targetWidth) / 2);

    for (int dy = 0; dy < targetHeight; ++dy) {
        const int sy = (dy * EmbeddedLogo::Height) / targetHeight;
        for (int dx = 0; dx < targetWidth; ++dx) {
            const int sx = (dx * EmbeddedLogo::Width) / targetWidth;
            if (LogoPixel(sx, sy)) {
                OSScreenPutPixelEx(screen,
                                   static_cast<uint32_t>(xOrigin + dx),
                                   static_cast<uint32_t>(yOrigin + dy),
                                   ScreenWhite);
            }
        }
    }
}

void DrawHorizontalLine(OSScreenID screen, int y, int width) {
    for (int x = 0; x < width; ++x) {
        OSScreenPutPixelEx(screen, static_cast<uint32_t>(x), static_cast<uint32_t>(y), SolarOrange);
        OSScreenPutPixelEx(screen, static_cast<uint32_t>(x), static_cast<uint32_t>(y + 1), SolarOrange);
    }
}

void DrawSelectionMarker(OSScreenID screen, int row) {
    const int y = row * 16 + 2;
    for (int py = 0; py < 12; ++py) {
        for (int px = 0; px < 5; ++px) {
            OSScreenPutPixelEx(screen,
                               static_cast<uint32_t>(6 + px),
                               static_cast<uint32_t>(y + py),
                               SolarOrange);
        }
    }
}

void PutClippedLine(OSScreenID screen, int columns, int x, int y, const char *line) {
    if (line == nullptr || x < 0 || x >= columns) return;
    char clipped[192] = {};
    std::snprintf(clipped, sizeof(clipped), "%s", line);
    const int available = columns - x;
    if (available <= 0) return;
    if (available < static_cast<int>(sizeof(clipped))) clipped[available] = '\0';
    OSScreenPutFontEx(screen, x, y, clipped);
}

void PrintScreen(OSScreenID screen, int columns, int x, int y, const char *format, va_list args) {
    char line[192] = {};
    vsnprintf(line, sizeof(line), format, args);
    PutClippedLine(screen, columns, x, y, line);
}

void PrintTV(int x, int y, const char *format, ...) {
    va_list args;
    va_start(args, format);
    PrintScreen(SCREEN_TV, TvTextColumns, x, y, format, args);
    va_end(args);
}

void PrintDRC(int x, int y, const char *format, ...) {
    va_list args;
    va_start(args, format);
    PrintScreen(SCREEN_DRC, DrcTextColumns, x, y, format, args);
    va_end(args);
}

bool InitScreen() {
    OSScreenInit();
    const uint32_t tvSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    const uint32_t drcSize = OSScreenGetBufferSizeEx(SCREEN_DRC);
    gTvBuffer = MEMAllocFromMappedMemoryForGX2Ex(tvSize, 0x100);
    gDrcBuffer = MEMAllocFromMappedMemoryForGX2Ex(drcSize, 0x100);

    if (gTvBuffer == nullptr || gDrcBuffer == nullptr) {
        if (gTvBuffer != nullptr) MEMFreeToMappedMemory(gTvBuffer);
        if (gDrcBuffer != nullptr) MEMFreeToMappedMemory(gDrcBuffer);
        gTvBuffer = nullptr;
        gDrcBuffer = nullptr;
        return false;
    }

    OSScreenSetBufferEx(SCREEN_TV, gTvBuffer);
    OSScreenSetBufferEx(SCREEN_DRC, gDrcBuffer);
    OSScreenEnableEx(SCREEN_TV, true);
    OSScreenEnableEx(SCREEN_DRC, true);
    return true;
}

void DeinitScreen() {
    if (gTvBuffer == nullptr && gDrcBuffer == nullptr) return;
    OSScreenClearBufferEx(SCREEN_TV, ScreenBlack);
    OSScreenClearBufferEx(SCREEN_DRC, ScreenBlack);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);

    if (gTvBuffer != nullptr) MEMFreeToMappedMemory(gTvBuffer);
    if (gDrcBuffer != nullptr) MEMFreeToMappedMemory(gDrcBuffer);
    gTvBuffer = nullptr;
    gDrcBuffer = nullptr;
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

uint32_t PollButtons() {
    uint32_t triggered = 0;
    VPADStatus vpad {};
    VPADReadError vpadError;
    VPADRead(VPAD_CHAN_0, &vpad, 1, &vpadError);
    if (vpadError == VPAD_READ_SUCCESS) triggered |= vpad.trigger;

    for (int channel = 0; channel < 4; ++channel) {
        KPADStatus status {};
        KPADError error;
        if (KPADReadEx(static_cast<KPADChan>(channel), &status, 1, &error) <= 0 ||
            error != KPAD_ERROR_OK || status.extensionType == 0xFF) continue;

        if (status.extensionType == WPAD_EXT_CORE || status.extensionType == WPAD_EXT_NUNCHUK)
            triggered |= RemapWiiRemoteButtons(status.trigger);
        else
            triggered |= RemapClassicButtons(status.classic.trigger);
    }
    return triggered;
}

size_t EnabledCount(const std::vector<ModInfo> &mods) {
    return static_cast<size_t>(std::count_if(mods.begin(), mods.end(), [](const ModInfo &mod) {
        return mod.enabled;
    }));
}

void RenderHeader() {
    if (EnsureLogoBitmap()) {
        DrawLogo(SCREEN_TV, TvWidth, TvLogoWidth, TvLogoHeight, 0);
        DrawLogo(SCREEN_DRC, DrcWidth, DrcLogoWidth, DrcLogoHeight, DrcLogoY);
    } else {
        PrintTV(2, 3, "SOLAR LAUNCHER");
        PrintDRC(2, 2, "SOLAR LAUNCHER");
    }

    DrawHorizontalLine(SCREEN_TV, TvHeaderSeparatorY, TvWidth);
    DrawHorizontalLine(SCREEN_TV, TvFooterSeparatorY, TvWidth);
    DrawHorizontalLine(SCREEN_DRC, DrcHeaderSeparatorY, DrcWidth);
    DrawHorizontalLine(SCREEN_DRC, DrcInfoSeparatorY, DrcWidth);
    DrawHorizontalLine(SCREEN_DRC, DrcControlsSeparatorY, DrcWidth);
}

void RenderDRCDetails(uint64_t titleId, const ModInfo &mod, size_t conflictCount, bool technical) {
    if (!technical) {
        PrintDRC(2, DrcInfoHeaderRow, "SELECTED MOD");
        PrintDRC(2, DrcInfoFirstRow + 0, "%.64s", mod.name.c_str());
        PrintDRC(2, DrcInfoFirstRow + 1, "By %.30s   v%.14s", mod.author.c_str(), mod.version.c_str());
        PrintDRC(2, DrcInfoFirstRow + 2, "Status:%s   Priority:%d   Conflicts:%u",
                 mod.enabled ? "ON" : "OFF", mod.priority,
                 static_cast<unsigned int>(conflictCount));
        PrintDRC(2, DrcInfoFirstRow + 3, "Type:%.12s   Source:%s", mod.type.c_str(),
                 mod.legacySDCafiine ? "SDCafiine" : "Solar");
        PrintDRC(2, DrcInfoFirstRow + 4, "Payload  content:%s  aoc:%s  patches:%s",
                 mod.hasContent ? "yes" : "no",
                 mod.hasAoc ? "yes" : "no",
                 mod.hasPatches ? "yes" : "no");
    } else {
        PrintDRC(2, DrcInfoHeaderRow, "TECHNICAL DETAILS");
        PrintDRC(2, DrcInfoFirstRow + 0, "Title: %s", TitleManager::FormatTitleId(titleId).c_str());
        PrintDRC(2, DrcInfoFirstRow + 1, "Folder: %.64s", mod.directoryName.c_str());
        PrintDRC(2, DrcInfoFirstRow + 2, "Current:%s P:%d   Default:%s P:%d",
                 mod.enabled ? "ON" : "OFF", mod.priority,
                 mod.defaultEnabled ? "ON" : "OFF", mod.defaultPriority);
        PrintDRC(2, DrcInfoFirstRow + 3, "content:%s  aoc:%s  patches:%s",
                 mod.hasContent ? "yes" : "no",
                 mod.hasAoc ? "yes" : "no",
                 mod.hasPatches ? "yes" : "no");
        PrintDRC(2, DrcInfoFirstRow + 4, "Source: %s", mod.legacySDCafiine ? "SDCafiine legacy pack" : "Solar mod");
    }
}

void RenderTVDetails(uint64_t titleId, const ModInfo &mod, size_t conflictCount, bool technical) {
    if (!technical) {
        PrintTV(TvInfoColumn, TvContentHeaderRow, "MOD INFORMATION");
        PrintTV(TvInfoColumn, TvContentFirstRow + 0, "Name: %-32.32s", mod.name.c_str());
        PrintTV(TvInfoColumn, TvContentFirstRow + 1, "Author: %-22.22s  v%.12s", mod.author.c_str(), mod.version.c_str());
        PrintTV(TvInfoColumn, TvContentFirstRow + 2, "Status:%s  Type:%.10s  Priority:%d", mod.enabled ? "ON" : "OFF", mod.type.c_str(), mod.priority);
        PrintTV(TvInfoColumn, TvContentFirstRow + 3, "Conflicts:%u  Source:%s", static_cast<unsigned int>(conflictCount), mod.legacySDCafiine ? "SDCafiine" : "Solar");
        PrintTV(TvInfoColumn, TvContentFirstRow + 4, "content:%s  aoc:%s  patches:%s", mod.hasContent ? "yes" : "no", mod.hasAoc ? "yes" : "no", mod.hasPatches ? "yes" : "no");
        PrintTV(TvInfoColumn, TvContentFirstRow + 5, "X: technical details");
    } else {
        PrintTV(TvInfoColumn, TvContentHeaderRow, "TECHNICAL DETAILS");
        PrintTV(TvInfoColumn, TvContentFirstRow + 0, "Title: %s", TitleManager::FormatTitleId(titleId).c_str());
        PrintTV(TvInfoColumn, TvContentFirstRow + 1, "Folder: %-34.34s", mod.directoryName.c_str());
        PrintTV(TvInfoColumn, TvContentFirstRow + 2, "Current:%s P:%d | Default:%s P:%d", mod.enabled ? "ON" : "OFF", mod.priority, mod.defaultEnabled ? "ON" : "OFF", mod.defaultPriority);
        PrintTV(TvInfoColumn, TvContentFirstRow + 3, "Source: %s", mod.legacySDCafiine ? "SDCafiine legacy pack" : "Solar mod");
        PrintTV(TvInfoColumn, TvContentFirstRow + 4, "content:%s  aoc:%s  patches:%s", mod.hasContent ? "yes" : "no", mod.hasAoc ? "yes" : "no", mod.hasPatches ? "yes" : "no");
        PrintTV(TvInfoColumn, TvContentFirstRow + 5, "X: mod information");
    }
}

void Render(uint64_t titleId,
            const std::vector<ModInfo> &mods,
            size_t selected,
            const ConflictReport &conflicts,
            bool technical,
            const char *notice = nullptr) {
    OSScreenClearBufferEx(SCREEN_TV, ScreenBlack);
    OSScreenClearBufferEx(SCREEN_DRC, ScreenBlack);
    RenderHeader();

    const size_t page = selected / ItemsPerPage;
    const size_t start = page * ItemsPerPage;
    const size_t end = std::min(start + static_cast<size_t>(ItemsPerPage), mods.size());
    const size_t pages = std::max<size_t>(1, (mods.size() + ItemsPerPage - 1) / ItemsPerPage);
    const size_t enabled = EnabledCount(mods);

    PrintTV(1, TvContentHeaderRow, "MODS  %u/%u enabled   Page %u/%u",
            static_cast<unsigned int>(enabled), static_cast<unsigned int>(mods.size()),
            static_cast<unsigned int>(page + 1), static_cast<unsigned int>(pages));

    int tvRow = TvContentFirstRow;
    for (size_t index = start; index < end; ++index, ++tvRow) {
        const ModInfo &mod = mods[index];
        const size_t conflictCount = index < conflicts.perMod.size() ? conflicts.perMod[index] : 0;
        if (index == selected) DrawSelectionMarker(SCREEN_TV, tvRow);
        PrintTV(2, tvRow, "%c [%s] %-25.25s", index == selected ? '>' : ' ', mod.enabled ? "ON " : "OFF", mod.name.c_str());
        if (conflictCount > 0) PrintTV(35, tvRow, "!%u", static_cast<unsigned int>(conflictCount));
        else if (mod.legacySDCafiine) PrintTV(35, tvRow, "SDC");
    }

    if (!mods.empty()) {
        const size_t conflictCount = selected < conflicts.perMod.size() ? conflicts.perMod[selected] : 0;
        RenderTVDetails(titleId, mods[selected], conflictCount, technical);
    }

    PrintTV(1, TvFooterFirstRow + 0, "A Toggle   X Details   L/R Priority   Y Reset");
    PrintTV(1, TvFooterFirstRow + 1, "+ Save & launch mods   B Launch vanilla once");
    PrintTV(1, TvFooterFirstRow + 2, "SOLAR %s | %u/%u enabled | conflicts:%u%s | %s",
            LauncherVersion,
            static_cast<unsigned int>(enabled), static_cast<unsigned int>(mods.size()),
            static_cast<unsigned int>(conflicts.conflictingPaths), conflicts.truncated ? "+" : "",
            notice != nullptr ? notice : "READY");

    PrintDRC(2, DrcModsHeaderRow, "MODS  %u/%u ON                         PAGE %u/%u",
             static_cast<unsigned int>(enabled), static_cast<unsigned int>(mods.size()),
             static_cast<unsigned int>(page + 1), static_cast<unsigned int>(pages));

    int drcRow = DrcModsFirstRow;
    for (size_t index = start; index < end; ++index, ++drcRow) {
        const ModInfo &mod = mods[index];
        const size_t conflictCount = index < conflicts.perMod.size() ? conflicts.perMod[index] : 0;
        if (index == selected) DrawSelectionMarker(SCREEN_DRC, drcRow);
        PrintDRC(2, drcRow, "%c [%s] %.60s", index == selected ? '>' : ' ', mod.enabled ? "ON " : "OFF", mod.name.c_str());
        if (conflictCount > 0) PrintDRC(70, drcRow, "!%u", static_cast<unsigned int>(conflictCount));
        else if (mod.legacySDCafiine) PrintDRC(70, drcRow, "SDC");
    }

    if (!mods.empty()) {
        const size_t conflictCount = selected < conflicts.perMod.size() ? conflicts.perMod[selected] : 0;
        RenderDRCDetails(titleId, mods[selected], conflictCount, technical);
    }

    PrintDRC(2, DrcControlsFirstRow + 0, "A Toggle       X Details       L/R Priority");
    PrintDRC(2, DrcControlsFirstRow + 1, "Y Reset        + Launch mods   B Vanilla");
    PrintDRC(2, DrcControlsFirstRow + 2, "SOLAR %s   %u/%u enabled   Conflicts:%u%s",
             LauncherVersion,
             static_cast<unsigned int>(enabled), static_cast<unsigned int>(mods.size()),
             static_cast<unsigned int>(conflicts.conflictingPaths), conflicts.truncated ? "+" : "");
    PrintDRC(2, DrcControlsFirstRow + 3, "%s", notice != nullptr ? notice : "READY");

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

void RenderBootAnimation() {
    OSScreenClearBufferEx(SCREEN_TV, ScreenBlack);
    OSScreenClearBufferEx(SCREEN_DRC, ScreenBlack);
    RenderHeader();
    PrintTV(2, TvFooterFirstRow + 2, "SOLAR %s | STARTING...", LauncherVersion);
    PrintDRC(2, DrcControlsFirstRow + 3, "SOLAR %s | STARTING...", LauncherVersion);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
    OSSleepTicks(OSMillisecondsToTicks(120));
}

} // namespace

MenuResult ModMenu::Show(uint64_t titleId, std::vector<ModInfo> &mods) {
    if (mods.empty()) return {MenuAction::ApplySelected, false};
    if (!InitScreen()) {
        Logger::Error("Failed to initialize Solar pre-launch screen");
        return {MenuAction::Failed, false};
    }

    RenderBootAnimation();
    KPADInit();
    WPADEnableURCC(true);

    size_t selected = 0;
    bool changed = false;
    bool technical = false;
    bool dirty = true;
    ConflictReport conflicts = ConflictDetector::Analyze(mods);

    while (true) {
        if (dirty) {
            Render(titleId, mods, selected, conflicts, technical);
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
            technical = !technical;
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
            Render(titleId, mods, selected, conflicts, technical,
                   saved ? "SAVED - LAUNCHING MODS..." : "SAVE FAILED - LAUNCHING THIS SESSION...");
            OSSleepTicks(OSMillisecondsToTicks(140));
            if (!saved) Logger::Warn("Selection could not be saved; applying it for this launch only");
            KPADShutdown();
            DeinitScreen();
            return {MenuAction::ApplySelected, changed};
        } else if (buttons & VPAD_BUTTON_B) {
            Render(titleId, mods, selected, conflicts, technical, "LAUNCHING VANILLA...");
            OSSleepTicks(OSMillisecondsToTicks(140));
            KPADShutdown();
            DeinitScreen();
            return {MenuAction::LaunchVanilla, changed};
        }

        OSSleepTicks(OSMillisecondsToTicks(16));
    }
}

} // namespace Solar
