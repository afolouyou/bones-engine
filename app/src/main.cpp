// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Xenity Engine

#include <engine/engine.h>
#include <engine/debug/debug.h>

#if defined(__PS3__)
#include <sys/thread.h>
#include <unistd.h>
// #include <sys/process.h>
// SYS_PROCESS_PARAM(1001, 0x100000); // Crash on real PS3, why?
#endif

//------------------------------- Link to the API documentation: https://fewnity.github.io/Xenity-Engine/script_api_reference/scripting_api_reference.html

#undef main

int main(int argc, char* argv[])
{
	// Init engine
	const int engineInitResult = Engine::Init(argc, argv);
	if (engineInitResult != 0)
	{
		Debug::PrintError("-------- Engine failed to init --------", true);
		return -1;
	}

	// Engine and game loop
	Engine::Loop();
	Debug::Print("-------- Game loop ended --------", true);
	Engine::Stop();

	// Weird fix for PS3 to avoid crash on exit
#if defined(__PS3__)
	sleep(3);
#endif

	return 0;
}
