// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include <bones/api.h>

#include <cstdio>
#include <cstring>

#if defined(__PS3__)
#include <sys/thread.h>
#include <unistd.h>
// #include <sys/process.h>
// SYS_PROCESS_PARAM(1001, 0x100000); // Crash on real PS3, why?
#endif

//------------------------------- Link to the API documentation: https://github.com/afolouyou/bones-engine

int main(int argc, char* argv[])
{
	if (argc > 1 && (std::strcmp(argv[1], "--version") == 0 || std::strcmp(argv[1], "-v") == 0))
	{
		std::printf("%s %s\n", bones_engine_name(), bones_version());
		return 0;
	}

	// Init engine through the stable C ABI.
	const int engineInitResult = bones_init(argc, argv);
	if (engineInitResult != 0)
	{
		bones_log(BONES_LOG_ERROR, "-------- Engine failed to init --------");
		return -1;
	}

	// Engine and game loop
	bones_run();
	bones_log(BONES_LOG_INFO, "-------- Game loop ended --------");
	bones_stop();

	// Weird fix for PS3 to avoid crash on exit
#if defined(__PS3__)
	sleep(3);
#endif

	return 0;
}
