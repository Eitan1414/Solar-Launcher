#include "solar/SelectionStore.hpp"
#include "solar/Logger.hpp"
#include "solar/Paths.hpp"
#include "solar/TitleManager.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>

namespace Solar::SelectionStore {
namespace {

struct Override {
    bool enabled = true;
    int priority = 0;
};

std::string ConfigPath(uint64_t titleId) {
    return std::string(Paths::ConfigRoot) + "/" + TitleManager::FormatTitleId(titleId) + ".cfg";
}

std::string Escape(const std::string &value) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size());

    for (unsigned char ch : value) {
        if (ch == '%' || ch == '\t' || ch == '\n' || ch == '\r') {
            out.push_back('%');
            out.push_back(hex[(ch >> 4) & 0x0F]);
            out.push_back(hex[ch & 0x0F]);
        } else {
            out.push_back(static_cast<char>(ch));
        }
    }

    return out;
}

int HexValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

std::string Unescape(const std::string &value) {
    std::string out;
    out.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const int hi = HexValue(value[i + 1]);
            const int lo = HexValue(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }

        out.push_back(value[i]);
    }

    return out;
}

std::string Key(bool legacy, const std::string &directoryName) {
    return std::string(legacy ? "L:" : "S:") + directoryName;
}

bool ParseLine(const std::string &line, std::string &keyOut, Override &valueOut) {
    const size_t p1 = line.find('\t');
    if (p1 == std::string::npos || p1 != 1) {
        return false;
    }

    const size_t p2 = line.find('\t', p1 + 1);
    const size_t p3 = line.find('\t', p2 == std::string::npos ? p2 : p2 + 1);
    if (p2 == std::string::npos || p3 == std::string::npos) {
        return false;
    }

    const char source = line[0];
    if (source != 'S' && source != 'L') {
        return false;
    }

    const std::string escapedName = line.substr(p1 + 1, p2 - p1 - 1);
    const std::string enabledText = line.substr(p2 + 1, p3 - p2 - 1);
    const std::string priorityText = line.substr(p3 + 1);

    char *end = nullptr;
    const long priority = std::strtol(priorityText.c_str(), &end, 10);
    if (end == priorityText.c_str()) {
        return false;
    }

    valueOut.enabled = enabledText == "1";
    valueOut.priority = static_cast<int>(priority);
    keyOut = Key(source == 'L', Unescape(escapedName));
    return true;
}

} // namespace

bool Load(uint64_t titleId, std::vector<ModInfo> &mods) {
    FILE *file = fopen(ConfigPath(titleId).c_str(), "rb");
    if (file == nullptr) {
        return false;
    }

    std::unordered_map<std::string, Override> overrides;
    char lineBuffer[1024];

    while (fgets(lineBuffer, sizeof(lineBuffer), file) != nullptr) {
        std::string line(lineBuffer);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }

        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::string key;
        Override value;
        if (ParseLine(line, key, value)) {
            overrides[key] = value;
        }
    }

    fclose(file);

    size_t applied = 0;
    for (auto &mod : mods) {
        const auto it = overrides.find(Key(mod.legacySDCafiine, mod.directoryName));
        if (it == overrides.end()) {
            continue;
        }

        mod.enabled = it->second.enabled;
        mod.priority = it->second.priority;
        ++applied;
    }

    Logger::Info("Loaded %u saved mod selection override(s) for %s",
                 static_cast<unsigned int>(applied),
                 TitleManager::FormatTitleId(titleId).c_str());
    return true;
}

bool Save(uint64_t titleId, const std::vector<ModInfo> &mods) {
    const std::string path = ConfigPath(titleId);
    FILE *file = fopen(path.c_str(), "wb");
    if (file == nullptr) {
        Logger::Error("Failed to open selection config %s", path.c_str());
        return false;
    }

    fprintf(file, "# Solar Launcher v0.3 selection state\n");
    fprintf(file, "# source<TAB>directory<TAB>enabled<TAB>priority\n");

    for (const auto &mod : mods) {
        fprintf(file, "%c\t%s\t%d\t%d\n",
                mod.legacySDCafiine ? 'L' : 'S',
                Escape(mod.directoryName).c_str(),
                mod.enabled ? 1 : 0,
                mod.priority);
    }

    const int closeResult = fclose(file);
    if (closeResult != 0) {
        Logger::Error("Failed to close selection config %s", path.c_str());
        return false;
    }

    Logger::Info("Saved selection state for %u mod(s)",
                 static_cast<unsigned int>(mods.size()));
    return true;
}

} // namespace Solar::SelectionStore
