/* Generated configuration header */
#ifndef CONFIG_H
#define CONFIG_H

#define BUILD_TYPE "Release"
#define PREFIX "/usr/local"

#ifdef __APPLE__
#define PLATFORM_MACOS 1
#elif __linux__
#define PLATFORM_LINUX 1
#endif

#define ENABLE_OVERLAY 1
#define ENABLE_TRAINER 1

#endif /* CONFIG_H */
