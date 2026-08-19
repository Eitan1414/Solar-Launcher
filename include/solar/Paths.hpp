#pragma once

#include <cstdint>
#include <string>

namespace Solar::Paths {

inline constexpr const char *SdRoot = "fs:/vol/external01/wiiu/SolarLauncher";
inline constexpr const char *GamesRoot = "fs:/vol/external01/wiiu/SolarLauncher/games";
inline constexpr const char *ConfigRoot = "fs:/vol/external01/wiiu/SolarLauncher/config";
inline constexpr const char *CacheRoot = "fs:/vol/external01/wiiu/SolarLauncher/cache";
inline constexpr const char *LogsRoot = "fs:/vol/external01/wiiu/SolarLauncher/logs";
inline constexpr const char *LogFile = "fs:/vol/external01/wiiu/SolarLauncher/logs/solar.log";
inline constexpr const char *SDCafiineRoot = "fs:/vol/external01/wiiu/sdcafiine";

bool EnsureBaseDirectories();
bool EnsureTitleDirectory(uint64_t titleId);
std::string TitleDirectory(uint64_t titleId);
std::string SDCafiineTitleDirectory(uint64_t titleId);

} // namespace Solar::Paths
