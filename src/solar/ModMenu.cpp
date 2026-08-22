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
#include <vector>

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
constexpr int TvCategoryRow = 15;
constexpr int TvContentHeaderRow = 16;
constexpr int TvContentFirstRow = 17;
constexpr int TvInfoHeaderRow = 16;
constexpr int TvInfoFirstRow = 17;
constexpr int TvInfoColumn = 43;
constexpr int TvFooterFirstRow = 24;

// GamePad uses its own spacious vertical layout. Categories get their own row,
// followed by four visible mods per page and then the selected-mod details.
constexpr int DrcLogoWidth = 500;
constexpr int DrcLogoHeight = (DrcLogoWidth * EmbeddedLogo::Height) / EmbeddedLogo::Width;
constexpr int DrcLogoY = 5;
constexpr int DrcHeaderSeparatorY = 150;
constexpr int DrcCategoryRow = 10;
constexpr int DrcModsHeaderRow = 11;
constexpr int DrcModsFirstRow = 12;
constexpr int DrcInfoSeparatorY = 260;
constexpr int DrcInfoHeaderRow = 17;
constexpr int DrcInfoFirstRow = 18;
constexpr int DrcControlsSeparatorY = 365;
constexpr int DrcControlsFirstRow = 24;

enum class ModCategory : uint8_t {
    All = 0,
    Mods,
    Textures,
    Behavior,
    Patches,
    Aoc,
    SDCafiine,
    Count
};

void *gTvBuffer = nullptr;
void *gDrcBuffer = nullptr;
uint8_t gLogoBitmap[EmbeddedLogo::BitmapSize] = {};
bool gLogoDecodeAttempted = false;
bool gLogoReady = false;

const char *CategoryLabel(ModCategory category) {
    switch (category) {
        case ModCategory::All: return "ALL";
        case ModCategory::Mods: return "MODS";
        case ModCategory::Textures: return "TEXTURES";
        case ModCategory::Behavior: return "BEHAVIOR";
        case ModCategory::Patches: return "PATCHES";
        case ModCategory::Aoc: return "AOC";
        case ModCategory::SDCafiine: return "SDCAFIINE";
        default: return "ALL";
    }
}

ModCategory StepCategory(ModCategory category, int direction) {
    const int count = static_cast<int>(ModCategory::Count);
    int value = static_cast<int>(category) + direction;
    if (value < 0) value = count - 1;
    if (value >= count) value = 0;
    return static_cast<ModCategory>(value);
}

bool MatchesCategory(const ModInfo &mod, ModCategory category) {
    switch (category) {
        case ModCategory::All:
            return true;
        case ModCategory::Mods:
            // General Solar mods. Special Texture/Behavior packs and legacy
            // SDCafiine packs have dedicated filters.
            return !mod.legacySDCafiine && !mod.hasTexturePack && !mod.hasBehaviorPack;
        case ModCategory::Textures:
            return mod.hasTexturePack;
        case ModCategory::Behavior:
            return mod.hasBehaviorPack;
        case ModCategory::Patches:
            return mod.hasPatches;
        case ModCategory::Aoc:
            return mod.hasAoc;
        case ModCategory::SDCafiine:
            return mod.legacySDCafiine;
        default:
            return true;
    }
}

std::vector<size_t> VisibleIndices(const std::vector<ModInfo> &mods, ModCategory category) {
    std::vector<size_t> indices;
    indices.reserve(mods.size());
    for (size_t index = 0; index < mods.size(); ++index) {
        if (MatchesCategory(mods[index], category)) indices.push_back(index);
    }
    return indices;
}

size_t VisiblePosition(const std::vector<size_t> &visible, size_t selected) {
    const auto it = std::find(visible.begin(), visible.end(), selected);
    return it == visible.end() ? 0 : static_cast<size_t>(std::distance(visible.begin(), it));
}

bool EnsureCategorySelection(const std::vector<ModInfo> &mods,
                             ModCategory category,
                             size_t &selected) {
    const auto visible = VisibleIndices(mods, category);
    if (visible.empty()) return false;
    if (std::find(visible.begin(), visible.end(), selected) == visible.end()) {
        selected = visible.front();
    }
    return true;
}

size_t EnabledCount(const std::vector<ModInfo> &mods) {
    return static_cast<size_t>(std::count_if(mods.begin(), mods.end(), [](const ModInfo &mod) {
        return mod.enabled;
    }));
}

size_t EnabledVisibleCount(const std::vector<ModInfo> &mods,
                           const std::vector<size_t> &visible) {
    size_t enabled = 0;
    for (const size_t index : visible) {
        if (index < mods.size() && mods[index].enabled) ++enabled;
    }
    return enabled;
}

std::string CategoryBar(ModCategory selected) {
    std::string bar = "FILTER: ";
    for (int value = 0; value < static_cast<int>(ModCategory::Count); ++value) {
        const auto category = static_cast<ModCategory>(value);
        const char *label = CategoryLabel(category);
        if (category == selected) {
            bar += "[";
            bar += label;
            bar += "]";
        } else {
            bar += label;
        }
        if (value + 1 < static_cast<int>(ModCategory::Count)) bar += "  ";
    }
    return bar;
}

const char *ModTypeTag(const ModInfo &mod) {
    if (mod.legacySDCafiine) return "SDC";
    if (mod.hasTexturePack && mod.hasBehaviorPack) return "T+B";
    if (mod.hasTexturePack) return "TEX";
    if (mod.hasBehaviorPack) return "BEH";
    if (mod.hasPatches) return "PAT";
    if (mod.hasAoc) return "AOC";
    return "MOD";
}

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
        PrintDRC(2, DrcInfoFirstRow + 3, "Type:%.14s   Source:%s", mod.type.c_str(),
                 mod.legacySDCafiine ? "SDCafiine" : "Solar");
        PrintDRC(2, DrcInfoFirstRow + 4, "Payload C:%s T:%s B:%s A:%s P:%s",
                 mod.hasContent ? "yes" : "no",
                 mod.hasTexturePack ? "yes" : "no",
                 mod.hasBehaviorPack ? "yes" : "no",
                 mod.hasAoc ? "yes" : "no",
                 mod.hasPatches ? "yes" : "no");
    } else {
        PrintDRC(2, DrcInfoHeaderRow, "TECHNICAL DETAILS");
        PrintDRC(2, DrcInfoFirstRow + 0, "Title: %s", TitleManager::FormatTitleId(titleId).c_str());
        PrintDRC(2, DrcInfoFirstRow + 1, "Folder: %.64s", mod.directoryName.c_str());
        PrintDRC(2, DrcInfoFirstRow + 2, "Current:%s P:%d   Default:%s P:%d",
                 mod.enabled ? "ON" : "OFF", mod.priority,
                 mod.defaultEnabled ? "ON" : "OFF", mod.defaultPriority);
        PrintDRC(2, DrcInfoFirstRow + 3, "C:%s T:%s B:%s A:%s P:%s",
                 mod.hasContent ? "yes" : "no",
                 mod.hasTexturePack ? "yes" : "no",
                 mod.hasBehaviorPack ? "yes" : "no",
                 mod.hasAoc ? "yes" : "no",
                 mod.hasPatches ? "yes" : "no");
        PrintDRC(2, DrcInfoFirstRow + 4, "Source: %s", mod.legacySDCafiine ? "SDCafiine legacy pack" : "Solar mod");
    }
}

void RenderTVDetails(uint64_t titleId, const ModInfo &mod, size_t conflictCount, bool technical) {
    if (!technical) {
        PrintTV(TvInfoColumn, TvInfoHeaderRow, "MOD INFORMATION");
        PrintTV(TvInfoColumn, TvInfoFirstRow + 0, "Name: %-32.32s", mod.name.c_str());
        PrintTV(TvInfoColumn, TvInfoFirstRow + 1, "Author: %-22.22s  v%.12s", mod.author.c_str(), mod.version.c_str());
        PrintTV(TvInfoColumn, TvInfoFirstRow + 2, "Status:%s  Type:%.14s  Priority:%d", mod.enabled ? "ON" : "OFF", mod.type.c_str(), mod.priority);
        PrintTV(TvInfoColumn, TvInfoFirstRow + 3, "Conflicts:%u  Source:%s", static_cast<unsigned int>(conflictCount), mod.legacySDCafiine ? "SDCafiine" : "Solar");
        PrintTV(TvInfoColumn, TvInfoFirstRow + 4, "C:%s T:%s B:%s A:%s P:%s",
                mod.hasContent ? "yes" : "no",
                mod.hasTexturePack ? "yes" : "no",
                mod.hasBehaviorPack ? "yes" : "no",
                mod.hasAoc ? "yes" : "no",
                mod.hasPatches ? "yes" : "no");
        PrintTV(TvInfoColumn, TvInfoFirstRow + 5, "X: technical details");
    } else {
        PrintTV(TvInfoColumn, TvInfoHeaderRow, "TECHNICAL DETAILS");
        PrintTV(TvInfoColumn, TvInfoFirstRow + 0, "Title: %s", TitleManager::FormatTitleId(titleId).c_str());
        PrintTV(TvInfoColumn, TvInfoFirstRow + 1, "Folder: %-34.34s", mod.directoryName.c_str());
        PrintTV(TvInfoColumn, TvInfoFirstRow + 2, "Current:%s P:%d | Default:%s P:%d", mod.enabled ? "ON" : "OFF", mod.priority, mod.defaultEnabled ? "ON" : "OFF", mod.defaultPriority);
        PrintTV(TvInfoColumn, TvInfoFirstRow + 3, "Source: %s", mod.legacySDCafiine ? "SDCafiine legacy pack" : "Solar mod");
        PrintTV(TvInfoColumn, TvInfoFirstRow + 4, "C:%s T:%s B:%s A:%s P:%s",
                mod.hasContent ? "yes" : "no",
                mod.hasTexturePack ? "yes" : "no",
                mod.hasBehaviorPack ? "yes" : "no",
                mod.hasAoc ? "yes" : "no",
                mod.hasPatches ? "yes" : "no");
        PrintTV(TvInfoColumn, TvInfoFirstRow + 5, "X: mod information");
    }
}

void RenderEmptyCategory(ModCategory category) {
    PrintTV(2, TvContentFirstRow, "No mods in %s", CategoryLabel(category));
    PrintTV(TvInfoColumn, TvInfoHeaderRow, "NO MOD SELECTED");
    PrintTV(TvInfoColumn, TvInfoFirstRow, "Use LEFT/RIGHT to change category.");

    PrintDRC(2, DrcModsFirstRow, "No mods in %s", CategoryLabel(category));
    PrintDRC(2, DrcInfoHeaderRow, "NO MOD SELECTED");
    PrintDRC(2, DrcInfoFirstRow, "Use LEFT/RIGHT to change category.");
}

void Render(uint64_t titleId,
            const std::vector<ModInfo> &mods,
            size_t selected,
            ModCategory category,
            const ConflictReport &conflicts,
            bool technical,
            const char *notice = nullptr) {
    OSScreenClearBufferEx(SCREEN_TV, ScreenBlack);
    OSScreenClearBufferEx(SCREEN_DRC, ScreenBlack);
    RenderHeader();

    const auto visible = VisibleIndices(mods, category);
    const bool hasVisible = !visible.empty();
    const size_t visibleSelected = hasVisible ? VisiblePosition(visible, selected) : 0;
    const size_t page = hasVisible ? visibleSelected / ItemsPerPage : 0;
    const size_t start = page * ItemsPerPage;
    const size_t end = std::min(start + static_cast<size_t>(ItemsPerPage), visible.size());
    const size_t pages = std::max<size_t>(1, (visible.size() + ItemsPerPage - 1) / ItemsPerPage);
    const size_t enabledGlobal = EnabledCount(mods);
    const size_t enabledVisible = EnabledVisibleCount(mods, visible);
    const std::string categoryBar = CategoryBar(category);

    PrintTV(1, TvCategoryRow, "%s", categoryBar.c_str());
    PrintTV(1, TvContentHeaderRow, "%s  %u/%u ON   Page %u/%u",
            CategoryLabel(category),
            static_cast<unsigned int>(enabledVisible), static_cast<unsigned int>(visible.size()),
            static_cast<unsigned int>(page + 1), static_cast<unsigned int>(pages));

    int tvRow = TvContentFirstRow;
    for (size_t position = start; position < end; ++position, ++tvRow) {
        const size_t index = visible[position];
        const ModInfo &mod = mods[index];
        const size_t conflictCount = index < conflicts.perMod.size() ? conflicts.perMod[index] : 0;
        if (index == selected) DrawSelectionMarker(SCREEN_TV, tvRow);
        PrintTV(2, tvRow, "%c [%s][%s] %-19.19s",
                index == selected ? '>' : ' ', mod.enabled ? "ON " : "OFF",
                ModTypeTag(mod), mod.name.c_str());
        if (conflictCount > 0) PrintTV(38, tvRow, "!%u", static_cast<unsigned int>(conflictCount));
    }

    if (hasVisible) {
        const size_t actualSelected = visible[visibleSelected];
        const size_t conflictCount = actualSelected < conflicts.perMod.size() ? conflicts.perMod[actualSelected] : 0;
        RenderTVDetails(titleId, mods[actualSelected], conflictCount, technical);
    } else {
        RenderEmptyCategory(category);
    }

    PrintTV(1, TvFooterFirstRow + 0, "UP/DOWN Select   LEFT/RIGHT Category   A Toggle   X Details");
    PrintTV(1, TvFooterFirstRow + 1, "L/R Priority   Y Reset   + Launch mods   B Vanilla");
    PrintTV(1, TvFooterFirstRow + 2, "SOLAR %s | %u/%u enabled | %s:%u | conflicts:%u%s | %s",
            LauncherVersion,
            static_cast<unsigned int>(enabledGlobal), static_cast<unsigned int>(mods.size()),
            CategoryLabel(category), static_cast<unsigned int>(visible.size()),
            static_cast<unsigned int>(conflicts.conflictingPaths), conflicts.truncated ? "+" : "",
            notice != nullptr ? notice : "READY");

    PrintDRC(2, DrcCategoryRow, "%s", categoryBar.c_str());
    PrintDRC(2, DrcModsHeaderRow, "%s  %u/%u ON                         PAGE %u/%u",
             CategoryLabel(category),
             static_cast<unsigned int>(enabledVisible), static_cast<unsigned int>(visible.size()),
             static_cast<unsigned int>(page + 1), static_cast<unsigned int>(pages));

    int drcRow = DrcModsFirstRow;
    for (size_t position = start; position < end; ++position, ++drcRow) {
        const size_t index = visible[position];
        const ModInfo &mod = mods[index];
        const size_t conflictCount = index < conflicts.perMod.size() ? conflicts.perMod[index] : 0;
        if (index == selected) DrawSelectionMarker(SCREEN_DRC, drcRow);
        PrintDRC(2, drcRow, "%c [%s][%s] %.52s",
                 index == selected ? '>' : ' ', mod.enabled ? "ON " : "OFF",
                 ModTypeTag(mod), mod.name.c_str());
        if (conflictCount > 0) PrintDRC(72, drcRow, "!%u", static_cast<unsigned int>(conflictCount));
    }

    if (hasVisible) {
        const size_t actualSelected = visible[visibleSelected];
        const size_t conflictCount = actualSelected < conflicts.perMod.size() ? conflicts.perMod[actualSelected] : 0;
        RenderDRCDetails(titleId, mods[actualSelected], conflictCount, technical);
    }

    PrintDRC(2, DrcControlsFirstRow + 0, "UP/DOWN Select    LEFT/RIGHT Category    A Toggle");
    PrintDRC(2, DrcControlsFirstRow + 1, "X Details   L/R Priority   Y Reset   + Launch   B Vanilla");
    PrintDRC(2, DrcControlsFirstRow + 2, "SOLAR %s   %u/%u enabled   %s:%u   Conflicts:%u%s",
             LauncherVersion,
             static_cast<unsigned int>(enabledGlobal), static_cast<unsigned int>(mods.size()),
             CategoryLabel(category), static_cast<unsigned int>(visible.size()),
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
    ModCategory category = ModCategory::All;
    bool changed = false;
    bool technical = false;
    bool dirty = true;
    ConflictReport conflicts = ConflictDetector::Analyze(mods);

    while (true) {
        EnsureCategorySelection(mods, category, selected);

        if (dirty) {
            Render(titleId, mods, selected, category, conflicts, technical);
            dirty = false;
        }

        const uint32_t buttons = PollButtons();
        if (buttons & VPAD_BUTTON_LEFT) {
            category = StepCategory(category, -1);
            EnsureCategorySelection(mods, category, selected);
            technical = false;
            dirty = true;
        } else if (buttons & VPAD_BUTTON_RIGHT) {
            category = StepCategory(category, 1);
            EnsureCategorySelection(mods, category, selected);
            technical = false;
            dirty = true;
        } else if (buttons & VPAD_BUTTON_UP) {
            const auto visible = VisibleIndices(mods, category);
            if (!visible.empty()) {
                const size_t position = VisiblePosition(visible, selected);
                selected = visible[position == 0 ? visible.size() - 1 : position - 1];
                dirty = true;
            }
        } else if (buttons & VPAD_BUTTON_DOWN) {
            const auto visible = VisibleIndices(mods, category);
            if (!visible.empty()) {
                const size_t position = VisiblePosition(visible, selected);
                selected = visible[(position + 1) % visible.size()];
                dirty = true;
            }
        } else if (buttons & VPAD_BUTTON_A) {
            if (EnsureCategorySelection(mods, category, selected)) {
                mods[selected].enabled = !mods[selected].enabled;
                changed = true;
                conflicts = ConflictDetector::Analyze(mods);
                dirty = true;
            }
        } else if (buttons & VPAD_BUTTON_X) {
            if (EnsureCategorySelection(mods, category, selected)) {
                technical = !technical;
                dirty = true;
            }
        } else if (buttons & VPAD_BUTTON_L) {
            if (EnsureCategorySelection(mods, category, selected)) {
                mods[selected].priority = std::max(MinPriority, mods[selected].priority - PriorityStep);
                changed = true;
                dirty = true;
            }
        } else if (buttons & VPAD_BUTTON_R) {
            if (EnsureCategorySelection(mods, category, selected)) {
                mods[selected].priority = std::min(MaxPriority, mods[selected].priority + PriorityStep);
                changed = true;
                dirty = true;
            }
        } else if (buttons & VPAD_BUTTON_Y) {
            if (EnsureCategorySelection(mods, category, selected)) {
                mods[selected].enabled = mods[selected].defaultEnabled;
                mods[selected].priority = mods[selected].defaultPriority;
                changed = true;
                conflicts = ConflictDetector::Analyze(mods);
                dirty = true;
            }
        } else if (buttons & VPAD_BUTTON_PLUS) {
            const bool saved = SelectionStore::Save(titleId, mods);
            Render(titleId, mods, selected, category, conflicts, technical,
                   saved ? "SAVED - LAUNCHING MODS..." : "SAVE FAILED - LAUNCHING THIS SESSION...");
            OSSleepTicks(OSMillisecondsToTicks(140));
            if (!saved) Logger::Warn("Selection could not be saved; applying it for this launch only");
            KPADShutdown();
            DeinitScreen();
            return {MenuAction::ApplySelected, changed};
        } else if (buttons & VPAD_BUTTON_B) {
            Render(titleId, mods, selected, category, conflicts, technical, "LAUNCHING VANILLA...");
            OSSleepTicks(OSMillisecondsToTicks(140));
            KPADShutdown();
            DeinitScreen();
            return {MenuAction::LaunchVanilla, changed};
        }

        OSSleepTicks(OSMillisecondsToTicks(16));
    }
}

} // namespace Solar
