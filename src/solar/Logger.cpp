#include "solar/Logger.hpp"
#include "solar/Paths.hpp"

#include <coreinit/debug.h>
#include <cstdarg>
#include <cstdio>

namespace Solar::Logger {
namespace {

void Write(const char *level, const char *format, va_list args) {
    char message[1024] = {};
    vsnprintf(message, sizeof(message), format, args);

    OSReport("[Solar][%s] %s\n", level, message);

    FILE *file = fopen(Paths::LogFile, "a");
    if (file != nullptr) {
        fprintf(file, "[Solar][%s] %s\n", level, message);
        fclose(file);
    }
}

void WriteVariadic(const char *level, const char *format, va_list args) {
    va_list copy;
    va_copy(copy, args);
    Write(level, format, copy);
    va_end(copy);
}

} // namespace

void Info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    WriteVariadic("INFO", format, args);
    va_end(args);
}

void Warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    WriteVariadic("WARN", format, args);
    va_end(args);
}

void Error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    WriteVariadic("ERROR", format, args);
    va_end(args);
}

} // namespace Solar::Logger
