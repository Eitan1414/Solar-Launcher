#include "solar/ModManager.hpp"
#include "solar/Logger.hpp"
#include "solar/Paths.hpp"
#include "solar/TitleManager.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <utility>

namespace Solar {
namespace {

constexpr long MaxManifestSize = 64 * 1024;
constexpr int LegacySDCafiinePriority = -1000;

bool IsDirectory(const std::string &path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

bool FileExists(const std::string &path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
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
    if (size < 0 || size > MaxManifestSize) {
        fclose(file);
        return {};
    }

    rewind(file);
    std::string result(static_cast<size_t>(size), '\0');
    if (size > 0) {
        const size_t bytesRead = fread(result.data(), 1, static_cast<size_t>(size), file);
        result.resize(bytesRead);
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

void PopulateReplacementFlags(ModInfo &mod) {
    mod.hasContent = IsDirectory(mod.path + "/content");
    mod.hasAoc = IsDirectory(mod.path + "/aoc");
}

ModInfo ParseManifest(const std::string &directoryName, const std::string &directoryPath,
                      const std::string &manifest) {
    ModInfo mod;
    mod.directoryName = directoryName;
    mod.path = directoryPath;
    mod.name = ExtractJsonString(manifest, "name");
    mod.author = ExtractJsonString(manifest, "author");
    mod.version = ExtractJsonString(manifest, "version");
    mod.type = ExtractJsonString(manifest, "type");
    mod.declaredTitleId = ExtractJsonString(manifest, "titleId");
    mod.enabled = ExtractJsonBool(manifest, "enabled", true);
    mod.priority = ExtractJsonInt(manifest, "priority", 0);
    mod.defaultEnabled = mod.enabled;
    mod.defaultPriority = mod.priority;

    if (mod.name.empty()) {
        mod.name = directoryName;
    }
    if (mod.author.empty()) {
        mod.author = "Unknown";
    }
    if (mod.version.empty()) {
        mod.version = "Unknown";
    }
    if (mod.type.empty()) {
        mod.type = "unknown";
    }

    PopulateReplacementFlags(mod);
    return mod;
}

void ScanSolarMods(uint64_t titleId, std::vector<ModInfo> &mods) {
    const std::string currentTitleId = TitleManager::FormatTitleId(titleId);
    const std::string root = Paths::TitleDirectory(titleId);

    DIR *directory = opendir(root.c_str());
    if (directory == nullptr) {
        Logger::Info("No Solar mod directory for title %s", currentTitleId.c_str());
        return;
    }

    while (dirent *entry = readdir(directory)) {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." || name.empty() || name[0] == '.') {
            continue;
        }

        const std::string modPath = root + "/" + name;
        if (!IsDirectory(modPath)) {
            continue;
        }

        const std::string manifestPath = modPath + "/mod.json";
        if (!FileExists(manifestPath)) {
            Logger::Warn("Skipping %s: missing mod.json", modPath.c_str());
            continue;
        }

        const std::string manifest = ReadTextFile(manifestPath);
        if (manifest.empty()) {
            Logger::Warn("Skipping %s: mod.json is empty or unreadable", modPath.c_str());
            continue;
        }

        ModInfo mod = ParseManifest(name, modPath, manifest);
        if (!mod.declaredTitleId.empty()) {
            const std::string declared = NormalizeTitleId(mod.declaredTitleId);
            if (declared != currentTitleId) {
                Logger::Warn("Skipping %s: manifest Title ID %s does not match %s",
                             mod.name.c_str(), declared.c_str(), currentTitleId.c_str());
                continue;
            }
        }

        mods.push_back(std::move(mod));
    }

    closedir(directory);
}

void ScanLegacySDCafiineMods(uint64_t titleId, std::vector<ModInfo> &mods) {
    const std::string root = Paths::SDCafiineTitleDirectory(titleId);
    DIR *directory = opendir(root.c_str());
    if (directory == nullptr) {
        return;
    }

    std::vector<ModInfo> legacyMods;
    while (dirent *entry = readdir(directory)) {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." || name.empty() || name[0] == '.') {
            continue;
        }

        const std::string modPath = root + "/" + name;
        if (!IsDirectory(modPath)) {
            continue;
        }

        ModInfo mod;
        mod.directoryName = name;
        mod.path = modPath;
        mod.name = name;
        mod.author = "Unknown";
        mod.version = "SDCafiine legacy";
        mod.type = "sdcafiine";
        mod.declaredTitleId = TitleManager::FormatTitleId(titleId);
        mod.enabled = false;
        mod.priority = LegacySDCafiinePriority;
        mod.legacySDCafiine = true;
        PopulateReplacementFlags(mod);

        if (!mod.hasContent && !mod.hasAoc) {
            continue;
        }

        legacyMods.push_back(std::move(mod));
    }

    closedir(directory);

    if (legacyMods.size() == 1) {
        legacyMods[0].enabled = true;
        Logger::Info("Auto-enabled single SDCafiine pack: %s", legacyMods[0].name.c_str());
    } else if (legacyMods.size() > 1) {
        Logger::Info("Detected %u SDCafiine packs; Solar v0.3 selector will let the user choose.",
                     static_cast<unsigned int>(legacyMods.size()));
    }

    for (auto &mod : legacyMods) {
        mod.defaultEnabled = mod.enabled;
        mod.defaultPriority = mod.priority;
        mods.push_back(std::move(mod));
    }
}

} // namespace

std::vector<ModInfo> ModManager::ScanForTitle(uint64_t titleId, bool includeLegacySDCafiine) {
    std::vector<ModInfo> mods;
    ScanSolarMods(titleId, mods);

    if (includeLegacySDCafiine) {
        ScanLegacySDCafiineMods(titleId, mods);
    }

    return mods;
}

} // namespace Solar
