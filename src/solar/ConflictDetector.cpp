#include "solar/ConflictDetector.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Solar {
namespace {

constexpr size_t MaxTrackedFiles = 20000;
constexpr int MaxDepth = 32;

struct ScanState {
    std::unordered_map<std::string, size_t> firstOwner;
    std::unordered_set<std::string> conflicting;
    std::vector<size_t> perMod;
    size_t trackedFiles = 0;
    bool truncated = false;
};

bool IsDirectory(const std::string &path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

std::string NormalizeDeletedMarker(const std::string &relativePath) {
    const size_t slash = relativePath.find_last_of('/');
    const size_t leafPos = slash == std::string::npos ? 0 : slash + 1;
    constexpr const char *DeletedPrefix = ".deleted_";
    constexpr size_t DeletedPrefixLength = 9;

    if (relativePath.compare(leafPos, DeletedPrefixLength, DeletedPrefix) != 0) {
        return relativePath;
    }

    std::string normalized = relativePath;
    normalized.erase(leafPos, DeletedPrefixLength);
    return normalized;
}

void TrackFile(const std::string &targetPath, size_t owner, ScanState &state) {
    if (state.trackedFiles >= MaxTrackedFiles) {
        state.truncated = true;
        return;
    }

    ++state.trackedFiles;

    const auto [it, inserted] = state.firstOwner.emplace(targetPath, owner);
    if (inserted || it->second == owner) {
        return;
    }

    if (state.conflicting.insert(targetPath).second) {
        ++state.perMod[it->second];
    }
    ++state.perMod[owner];
}

void ScanDirectory(const std::string &basePath,
                   const std::string &relativePath,
                   const std::string &volumePrefix,
                   size_t owner,
                   int depth,
                   ScanState &state) {
    if (state.truncated || depth > MaxDepth) {
        if (depth > MaxDepth) {
            state.truncated = true;
        }
        return;
    }

    const std::string currentPath = relativePath.empty()
        ? basePath
        : basePath + "/" + relativePath;

    DIR *directory = opendir(currentPath.c_str());
    if (directory == nullptr) {
        return;
    }

    while (!state.truncated) {
        dirent *entry = readdir(directory);
        if (entry == nullptr) {
            break;
        }

        const std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }

        const std::string childRelative = relativePath.empty()
            ? name
            : relativePath + "/" + name;
        const std::string childPath = basePath + "/" + childRelative;

        struct stat info {};
        if (stat(childPath.c_str(), &info) != 0) {
            continue;
        }

        if (S_ISDIR(info.st_mode)) {
            ScanDirectory(basePath, childRelative, volumePrefix, owner, depth + 1, state);
        } else if (S_ISREG(info.st_mode)) {
            const std::string normalized = NormalizeDeletedMarker(childRelative);
            TrackFile(volumePrefix + "/" + normalized, owner, state);
        }
    }

    closedir(directory);
}

} // namespace

ConflictReport ConflictDetector::Analyze(const std::vector<ModInfo> &mods) {
    ScanState state;
    state.perMod.resize(mods.size(), 0);

    for (size_t index = 0; index < mods.size() && !state.truncated; ++index) {
        const ModInfo &mod = mods[index];
        if (!mod.enabled) {
            continue;
        }

        if (mod.hasContent && IsDirectory(mod.path + "/content")) {
            ScanDirectory(mod.path + "/content", "", "content", index, 0, state);
        }

        if (mod.hasAoc && IsDirectory(mod.path + "/aoc")) {
            ScanDirectory(mod.path + "/aoc", "", "aoc", index, 0, state);
        }
    }

    ConflictReport report;
    report.conflictingPaths = state.conflicting.size();
    report.perMod = std::move(state.perMod);
    report.truncated = state.truncated;
    return report;
}

} // namespace Solar
