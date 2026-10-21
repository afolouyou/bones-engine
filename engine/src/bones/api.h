// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#pragma once

/**
 * @file bones/api.h
 * @brief Stable C ABI for hosting Bones Engine.
 *
 * This header is the public boundary between the engine and host applications.
 * It must stay valid C and must not include any C++ header, so the same
 * functions can be consumed by C, C++, Zig, Nim or any other language with a
 * C FFI.
 */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(BONES_API_EXPORTS)
#define BONES_API __declspec(dllexport)
#elif defined(_WIN32) && defined(BONES_API_IMPORTS)
#define BONES_API __declspec(dllimport)
#elif defined(__GNUC__) && defined(BONES_API_EXPORTS)
#define BONES_API __attribute__((visibility("default")))
#else
#define BONES_API
#endif

/** Engine version, matching the public release label. */
#define BONES_VERSION_MAJOR 0
#define BONES_VERSION_MINOR 1
#define BONES_VERSION_PATCH 0

typedef enum BonesPlatform
{
	BONES_PLATFORM_UNKNOWN = 0,
	BONES_PLATFORM_WINDOWS = 1,
	BONES_PLATFORM_LINUX = 2,
	BONES_PLATFORM_PS3 = 3,
} BonesPlatform;

typedef enum BonesLogLevel
{
	BONES_LOG_INFO = 0,
	BONES_LOG_WARNING = 1,
	BONES_LOG_ERROR = 2,
} BonesLogLevel;

/** @brief Engine product name, for example "Bones Engine". Never NULL. */
BONES_API const char* bones_engine_name(void);

/** @brief Engine version string, for example "0.01". Never NULL. */
BONES_API const char* bones_version(void);

/** @brief Platform the engine was compiled for. */
BONES_API BonesPlatform bones_platform(void);

/**
 * @brief Initialize the engine and create its window.
 * @param argc Argument count, forwarded from main().
 * @param argv Argument vector, forwarded from main().
 * @return 0 on success, non-zero on failure.
 */
BONES_API int bones_init(int argc, char* argv[]);

/** @brief Run the engine main loop until bones_quit() or a window close. */
BONES_API void bones_run(void);

/** @brief Release engine resources after bones_run() returns. */
BONES_API void bones_stop(void);

/** @brief Ask the engine main loop to stop. Safe to call from game code. */
BONES_API void bones_quit(void);

/** @brief Return 1 while the engine is running, 0 otherwise. */
BONES_API int bones_is_running(void);

/**
 * @brief Write a message to the engine log.
 * @param level One of BonesLogLevel.
 * @param message UTF-8 text. NULL is ignored.
 */
BONES_API void bones_log(int level, const char* message);

#ifdef __cplusplus
}
#endif
