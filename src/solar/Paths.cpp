#include "solar/Paths.hpp"
#include "solar/Logger.hpp"
#include "solar/TitleManager.hpp"

#include <cerrno>
#include <sys/stat.h>

namespace Solar::Paths {
namespace {

bool EnsureDirectory(const char *path) {
    struct stat info {};
    if (stat(path, &info) == 0) {
        return S_ISDIR(info.st_mode);
    }

    if (mkdir(path, 0777) == 0 || errno == EEXIST) {
        return true;
    }

    Logger::Error("Failed to create directory %s (errno=%d)", path, errno);
    return false;
}

} // namespace

bool EnsureBaseDirectories() {
    bool ok = true;
    ok &= EnsureDirectory(SdRoot);
    ok &= EnsureDirectory(GamesRoot);
    ok &= EnsureDirectory(ConfigRoot);
    ok &= EnsureDirectory(CacheRoot);
    ok &= EnsureDirectory(LogsRoot);
    return ok;
}

std::string TitleDirectory(uint64_t titleId) {
    return std::string(GamesRoot) + "/" + TitleManager::FormatTitleId(titleId);
}

std::string SDCafiineTitleDirectory(uint64_t titleId) {
    return std::string(SDCafiineRoot) + "/" + TitleManager::FormatTitleId(titleId);
}

bool EnsureTitleDirectory(uint64_t titleId) {
    const auto path = TitleDirectory(titleId);
    return EnsureDirectory(path.c_str());
}

} // namespace Solar::Paths
