#include "solar/PatchEngine.hpp"

#include "solar/Logger.hpp"
#include "solar/NativeHookRegistry.hpp"
#include "solar/TitleManager.hpp"

#include <algorithm>
#include <cctype>
#include <coreinit/cache.h>
#include <coreinit/memorymap.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <function_patcher/function_patching.h>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace Solar::PatchEngine {
namespace {

constexpr long MaxPatchFileSize = 128 * 1024;
constexpr size_t MaxPatchBytes = 512;
constexpr size_t MaxPatchesPerFile = 128;
constexpr size_t MaxHooksPerFile = 64;

struct AppliedMemoryPatch {
    uint32_t address = 0;
    std::vector<uint8_t> original;
    bool executable = false;
    std::string label;
};

struct AppliedHook {
    PatchedFunctionHandle handle {};
    std::string id;
};

struct MemoryPatchRequest {
    std::string modName;
    std::string name;
    int priority = 0;
    uint32_t address = 0;
    std::vector<uint8_t> expected;
    std::vector<uint8_t> replacement;
    bool executable = false;
};

struct HookRequest {
    std::string modName;
    std::string id;
    int priority = 0;
};

struct OccupiedRange {
    uint32_t start = 0;
    uint32_t end = 0;
    std::string owner;
};

bool gInitialized = false;
bool gFunctionPatcherAvailable = false;
std::vector<AppliedMemoryPatch> gMemoryPatches;
std::vector<AppliedHook> gHooks;

bool IsDirectory(const std::string &path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

bool IsRegularFile(const std::string &path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

bool EndsWith(const std::string &value, const std::string &suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string ReadTextFile(const std::string &path) {
    FILE *file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return {};
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return {};
    }

    const long size = ftell(file);
    if (size < 0 || size > MaxPatchFileSize) {
        fclose(file);
        return {};
    }

    rewind(file);
    std::string result(static_cast<size_t>(size), '\0');
    if (size > 0) {
        const size_t read = fread(result.data(), 1, static_cast<size_t>(size), file);
        result.resize(read);
    }

    fclose(file);
    return result;
}

size_t FindJsonValue(const std::string &json, const std::string &key) {
    const std::string quotedKey = "\"" + key + "\"";
    size_t cursor = json.find(quotedKey);
    if (cursor == std::string::npos) {
        return std::string::npos;
    }

    cursor = json.find(':', cursor + quotedKey.size());
    if (cursor == std::string::npos) {
        return std::string::npos;
    }

    ++cursor;
    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
        ++cursor;
    }
    return cursor;
}

std::string ExtractJsonString(const std::string &json, const std::string &key) {
    size_t cursor = FindJsonValue(json, key);
    if (cursor == std::string::npos || cursor >= json.size() || json[cursor] != '"') {
        return {};
    }

    ++cursor;
    std::string value;
    bool escaped = false;

    for (; cursor < json.size(); ++cursor) {
        const char ch = json[cursor];
        if (escaped) {
            switch (ch) {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                default: value.push_back(ch); break;
            }
            escaped = false;
            continue;
        }

        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            break;
        }
        value.push_back(ch);
    }

    return value;
}

bool ExtractJsonBool(const std::string &json, const std::string &key, bool fallback) {
    const size_t cursor = FindJsonValue(json, key);
    if (cursor == std::string::npos) {
        return fallback;
    }

    if (json.compare(cursor, 4, "true") == 0) {
        return true;
    }
    if (json.compare(cursor, 5, "false") == 0) {
        return false;
    }
    return fallback;
}

int ExtractJsonInt(const std::string &json, const std::string &key, int fallback) {
    const size_t cursor = FindJsonValue(json, key);
    if (cursor == std::string::npos || cursor >= json.size()) {
        return fallback;
    }

    char *end = nullptr;
    const long value = std::strtol(json.c_str() + cursor, &end, 10);
    if (end == json.c_str() + cursor) {
        return fallback;
    }
    return static_cast<int>(value);
}

std::string NormalizeTitleId(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
        return !std::isxdigit(ch);
    }), value.end());

    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

std::string ExtractArrayBody(const std::string &json, const std::string &key) {
    size_t cursor = FindJsonValue(json, key);
    if (cursor == std::string::npos || cursor >= json.size() || json[cursor] != '[') {
        return {};
    }

    const size_t start = ++cursor;
    int depth = 1;
    bool inString = false;
    bool escaped = false;

    for (; cursor < json.size(); ++cursor) {
        const char ch = json[cursor];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
        } else if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                return json.substr(start, cursor - start);
            }
        }
    }

    return {};
}

std::vector<std::string> ExtractObjectArray(const std::string &arrayBody, size_t limit) {
    std::vector<std::string> objects;
    bool inString = false;
    bool escaped = false;
    int depth = 0;
    size_t objectStart = std::string::npos;

    for (size_t i = 0; i < arrayBody.size(); ++i) {
        const char ch = arrayBody[i];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            continue;
        }

        if (ch == '{') {
            if (depth == 0) {
                objectStart = i;
            }
            ++depth;
        } else if (ch == '}' && depth > 0) {
            --depth;
            if (depth == 0 && objectStart != std::string::npos) {
                objects.push_back(arrayBody.substr(objectStart, i - objectStart + 1));
                objectStart = std::string::npos;
                if (objects.size() >= limit) {
                    break;
                }
            }
        }
    }

    return objects;
}

std::vector<std::string> ExtractStringArray(const std::string &arrayBody, size_t limit) {
    std::vector<std::string> values;

    for (size_t i = 0; i < arrayBody.size() && values.size() < limit; ++i) {
        if (arrayBody[i] != '"') {
            continue;
        }

        ++i;
        std::string value;
        bool escaped = false;
        for (; i < arrayBody.size(); ++i) {
            const char ch = arrayBody[i];
            if (escaped) {
                value.push_back(ch);
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                break;
            } else {
                value.push_back(ch);
            }
        }

        if (!value.empty()) {
            values.push_back(std::move(value));
        }
    }

    return values;
}

int HexNibble(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

bool ParseHexBytes(const std::string &text, std::vector<uint8_t> &out) {
    std::string digits;
    digits.reserve(text.size());

    for (char ch : text) {
        if (std::isxdigit(static_cast<unsigned char>(ch))) {
            digits.push_back(ch);
        } else if (std::isspace(static_cast<unsigned char>(ch)) || ch == ':' || ch == '-' || ch == '_') {
            continue;
        } else {
            return false;
        }
    }

    if (digits.empty() || (digits.size() % 2) != 0 || digits.size() / 2 > MaxPatchBytes) {
        return false;
    }

    out.clear();
    out.reserve(digits.size() / 2);
    for (size_t i = 0; i < digits.size(); i += 2) {
        const int hi = HexNibble(digits[i]);
        const int lo = HexNibble(digits[i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

bool ParseAddress(const std::string &text, uint32_t &out) {
    if (text.empty()) {
        return false;
    }

    char *end = nullptr;
    const unsigned long value = std::strtoul(text.c_str(), &end, 0);
    if (end == text.c_str() || *end != '\0' || value > 0xFFFFFFFFul) {
        return false;
    }

    out = static_cast<uint32_t>(value);
    return true;
}

bool IsWritableRange(uint32_t address, size_t size) {
    if (size == 0 || size > 0xFFFFFFFFu) {
        return false;
    }

    const uint32_t length = static_cast<uint32_t>(size);
    if (address > 0xFFFFFFFFu - (length - 1u)) {
        return false;
    }

    const uint32_t end = address + length - 1u;
    if (!OSIsAddressValid(address) || !OSIsAddressValid(end)) {
        return false;
    }

    uint32_t cursor = address;
    while (true) {
        if (OSQueryVirtAddr(cursor) != OS_MAP_MEMORY_READ_WRITE) {
            return false;
        }

        const uint32_t pageBase = cursor & ~(static_cast<uint32_t>(OS_PAGE_SIZE) - 1u);
        const uint32_t nextPage = pageBase + static_cast<uint32_t>(OS_PAGE_SIZE);
        if (nextPage <= cursor || nextPage > end) {
            break;
        }
        cursor = nextPage;
    }

    return true;
}

bool RangesOverlap(uint32_t aStart, uint32_t aEnd, uint32_t bStart, uint32_t bEnd) {
    return aStart <= bEnd && bStart <= aEnd;
}

void WriteBytes(uint32_t address, const std::vector<uint8_t> &bytes, bool executable) {
    void *target = reinterpret_cast<void *>(static_cast<uintptr_t>(address));
    std::memcpy(target, bytes.data(), bytes.size());
    DCFlushRange(target, static_cast<uint32_t>(bytes.size()));
    if (executable) {
        ICInvalidateRange(target, static_cast<uint32_t>(bytes.size()));
    }
}

std::vector<std::string> ListPatchFiles(const std::string &directoryPath) {
    std::vector<std::string> files;
    DIR *directory = opendir(directoryPath.c_str());
    if (directory == nullptr) {
        return files;
    }

    while (dirent *entry = readdir(directory)) {
        const std::string name = entry->d_name;
        if (name.empty() || name[0] == '.' || !EndsWith(name, ".json")) {
            continue;
        }

        const std::string path = directoryPath + "/" + name;
        if (IsRegularFile(path)) {
            files.push_back(path);
        }
    }

    closedir(directory);
    std::sort(files.begin(), files.end());
    return files;
}

void ParsePatchFile(uint64_t titleId,
                    const ModInfo &mod,
                    const std::string &path,
                    std::vector<MemoryPatchRequest> &memoryRequests,
                    std::vector<HookRequest> &hookRequests,
                    PatchApplyReport &report) {
    ++report.patchFiles;

    const std::string json = ReadTextFile(path);
    if (json.empty()) {
        Logger::Warn("Skipping unreadable patch file: %s", path.c_str());
        ++report.memoryPatchesFailed;
        return;
    }

    const int formatVersion = ExtractJsonInt(json, "formatVersion", 1);
    if (formatVersion != 1) {
        Logger::Warn("Skipping %s: unsupported patch format version %d", path.c_str(), formatVersion);
        ++report.memoryPatchesFailed;
        return;
    }

    if (!ExtractJsonBool(json, "enabled", true)) {
        Logger::Info("Patch file disabled: %s", path.c_str());
        return;
    }

    const std::string declaredTitle = ExtractJsonString(json, "titleId");
    if (!declaredTitle.empty() && NormalizeTitleId(declaredTitle) != TitleManager::FormatTitleId(titleId)) {
        Logger::Warn("Skipping %s: patch Title ID does not match running title", path.c_str());
        ++report.memoryPatchesFailed;
        return;
    }

    const auto patchObjects = ExtractObjectArray(ExtractArrayBody(json, "patches"), MaxPatchesPerFile);
    for (const std::string &object : patchObjects) {
        ++report.memoryPatchesFound;

        if (!ExtractJsonBool(object, "enabled", true)) {
            ++report.memoryPatchesSkipped;
            continue;
        }

        MemoryPatchRequest request;
        request.modName = mod.name;
        request.name = ExtractJsonString(object, "name");
        request.priority = mod.priority;
        request.executable = ExtractJsonBool(object, "executable", false);

        if (request.name.empty()) {
            request.name = "unnamed patch";
        }

        const std::string addressText = ExtractJsonString(object, "address");
        const std::string expectedText = ExtractJsonString(object, "expected");
        const std::string replacementText = ExtractJsonString(object, "replace");

        if (!ParseAddress(addressText, request.address) ||
            !ParseHexBytes(expectedText, request.expected) ||
            !ParseHexBytes(replacementText, request.replacement) ||
            request.expected.size() != request.replacement.size()) {
            Logger::Warn("Invalid memory patch %s in %s", request.name.c_str(), path.c_str());
            ++report.memoryPatchesFailed;
            continue;
        }

        memoryRequests.push_back(std::move(request));
    }

    const auto hooks = ExtractStringArray(ExtractArrayBody(json, "hooks"), MaxHooksPerFile);
    for (const std::string &id : hooks) {
        ++report.hookRequestsFound;
        hookRequests.push_back({mod.name, id, mod.priority});
    }
}

} // namespace

bool Initialize() {
    if (gInitialized) {
        return true;
    }

    gInitialized = true;
    const FunctionPatcherStatus status = FunctionPatcher_InitLibrary();
    if (status == FUNCTION_PATCHER_RESULT_SUCCESS) {
        gFunctionPatcherAvailable = true;
        Logger::Info("FunctionPatcher initialized for Solar Patch Engine");
    } else {
        gFunctionPatcherAvailable = false;
        Logger::Warn("FunctionPatcher unavailable: %s (%d). Native hooks will be skipped.",
                     FunctionPatcher_GetStatusStr(status), status);
    }

    Logger::Info("Solar Patch Engine initialized");
    return true;
}

void Clear() {
    if (gFunctionPatcherAvailable) {
        for (auto it = gHooks.rbegin(); it != gHooks.rend(); ++it) {
            const FunctionPatcherStatus status = FunctionPatcher_RemoveFunctionPatch(it->handle);
            if (status != FUNCTION_PATCHER_RESULT_SUCCESS) {
                Logger::Warn("Failed to remove native hook %s: %s (%d)",
                             it->id.c_str(), FunctionPatcher_GetStatusStr(status), status);
            }
        }
    }
    gHooks.clear();

    for (auto it = gMemoryPatches.rbegin(); it != gMemoryPatches.rend(); ++it) {
        if (!IsWritableRange(it->address, it->original.size())) {
            Logger::Warn("Cannot restore patch %s: memory range is no longer writable", it->label.c_str());
            continue;
        }
        WriteBytes(it->address, it->original, it->executable);
        Logger::Info("Restored memory patch: %s", it->label.c_str());
    }
    gMemoryPatches.clear();
}

void Shutdown() {
    if (!gInitialized) {
        return;
    }

    Clear();

    if (gFunctionPatcherAvailable) {
        const FunctionPatcherStatus status = FunctionPatcher_DeInitLibrary();
        if (status != FUNCTION_PATCHER_RESULT_SUCCESS) {
            Logger::Warn("FunctionPatcher deinit returned %s (%d)",
                         FunctionPatcher_GetStatusStr(status), status);
        }
    }

    NativeHookRegistry::ClearRegistrations();
    gFunctionPatcherAvailable = false;
    gInitialized = false;
    Logger::Info("Solar Patch Engine shut down");
}

bool IsInitialized() {
    return gInitialized;
}

bool IsFunctionPatcherAvailable() {
    return gFunctionPatcherAvailable;
}

PatchApplyReport Apply(uint64_t titleId, const std::vector<ModInfo> &mods) {
    PatchApplyReport report;
    Clear();

    if (!gInitialized) {
        Logger::Warn("Patch Engine is not initialized");
        return report;
    }

    std::vector<MemoryPatchRequest> memoryRequests;
    std::vector<HookRequest> hookRequests;

    for (const ModInfo &mod : mods) {
        if (!mod.enabled || !mod.hasPatches || mod.legacySDCafiine) {
            continue;
        }

        const std::string patchDirectory = mod.path + "/patches";
        if (!IsDirectory(patchDirectory)) {
            continue;
        }

        for (const std::string &file : ListPatchFiles(patchDirectory)) {
            ParsePatchFile(titleId, mod, file, memoryRequests, hookRequests, report);
        }
    }

    std::stable_sort(memoryRequests.begin(), memoryRequests.end(), [](const MemoryPatchRequest &lhs, const MemoryPatchRequest &rhs) {
        if (lhs.priority != rhs.priority) {
            return lhs.priority > rhs.priority;
        }
        if (lhs.modName != rhs.modName) {
            return lhs.modName < rhs.modName;
        }
        return lhs.name < rhs.name;
    });

    std::vector<OccupiedRange> occupied;
    for (const MemoryPatchRequest &request : memoryRequests) {
        const uint32_t end = request.address + static_cast<uint32_t>(request.expected.size()) - 1u;

        auto conflict = std::find_if(occupied.begin(), occupied.end(), [&](const OccupiedRange &range) {
            return RangesOverlap(request.address, end, range.start, range.end);
        });

        if (conflict != occupied.end()) {
            Logger::Warn("Skipping patch %s from %s: overlaps higher-priority patch %s",
                         request.name.c_str(), request.modName.c_str(), conflict->owner.c_str());
            ++report.memoryPatchesSkipped;
            continue;
        }

        if (!IsWritableRange(request.address, request.expected.size())) {
            Logger::Warn("Skipping patch %s from %s: 0x%08X is not a writable mapped range",
                         request.name.c_str(), request.modName.c_str(), request.address);
            ++report.memoryPatchesFailed;
            continue;
        }

        std::vector<uint8_t> current(request.expected.size());
        std::memcpy(current.data(), reinterpret_cast<const void *>(static_cast<uintptr_t>(request.address)), current.size());

        if (current != request.expected) {
            Logger::Warn("Skipping patch %s from %s: expected bytes do not match at 0x%08X",
                         request.name.c_str(), request.modName.c_str(), request.address);
            ++report.memoryPatchesSkipped;
            continue;
        }

        AppliedMemoryPatch applied;
        applied.address = request.address;
        applied.original = current;
        applied.executable = request.executable;
        applied.label = request.modName + ": " + request.name;

        WriteBytes(request.address, request.replacement, request.executable);
        gMemoryPatches.push_back(std::move(applied));
        occupied.push_back({request.address, end, request.modName + ": " + request.name});
        ++report.memoryPatchesApplied;

        Logger::Info("Applied memory patch %s from %s at 0x%08X (%u bytes)",
                     request.name.c_str(), request.modName.c_str(), request.address,
                     static_cast<unsigned int>(request.replacement.size()));
    }

    std::stable_sort(hookRequests.begin(), hookRequests.end(), [](const HookRequest &lhs, const HookRequest &rhs) {
        if (lhs.priority != rhs.priority) {
            return lhs.priority < rhs.priority;
        }
        return lhs.id < rhs.id;
    });

    for (const HookRequest &request : hookRequests) {
        if (!gFunctionPatcherAvailable) {
            ++report.hooksSkipped;
            continue;
        }

        PatchedFunctionHandle handle {};
        bool initiallyPatched = false;
        const NativeHookRegistry::ApplyResult result = NativeHookRegistry::Apply(
            request.id, &handle, &initiallyPatched);

        if (result == NativeHookRegistry::ApplyResult::NotRegistered) {
            Logger::Warn("Skipping hook %s requested by %s: no Solar Game Adapter registered it",
                         request.id.c_str(), request.modName.c_str());
            ++report.hooksSkipped;
            continue;
        }
        if (result == NativeHookRegistry::ApplyResult::Failed) {
            ++report.hooksSkipped;
            continue;
        }

        gHooks.push_back({handle, request.id});
        ++report.hooksApplied;
        Logger::Info("Added native hook %s for %s (initially patched=%s)",
                     request.id.c_str(), request.modName.c_str(), initiallyPatched ? "yes" : "no");
    }

    Logger::Info("Patch Engine: files=%u memory=%u/%u hooks=%u/%u",
                 static_cast<unsigned int>(report.patchFiles),
                 static_cast<unsigned int>(report.memoryPatchesApplied),
                 static_cast<unsigned int>(report.memoryPatchesFound),
                 static_cast<unsigned int>(report.hooksApplied),
                 static_cast<unsigned int>(report.hookRequestsFound));

    return report;
}

} // namespace Solar::PatchEngine
