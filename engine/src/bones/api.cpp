// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include <bones/api.h>

#include <engine/constants.h>
#include <engine/debug/debug.h>
#include <engine/engine.h>

extern "C"
{

	const char* bones_engine_name(void)
	{
		return "Bones Engine";
	}

	const char* bones_version(void)
	{
		return ENGINE_VERSION;
	}

	BonesPlatform bones_platform(void)
	{
#if defined(__PS3__)
		return BONES_PLATFORM_PS3;
#elif defined(_WIN32) || defined(_WIN64)
		return BONES_PLATFORM_WINDOWS;
#elif defined(__linux__) || defined(__LINUX__)
		return BONES_PLATFORM_LINUX;
#else
		return BONES_PLATFORM_UNKNOWN;
#endif
	}

	int bones_init(int argc, char* argv[])
	{
		return Engine::Init(argc, argv);
	}

	void bones_run(void)
	{
		Engine::Loop();
	}

	void bones_stop(void)
	{
		Engine::Stop();
	}

	void bones_quit(void)
	{
		Engine::Quit();
	}

	int bones_is_running(void)
	{
		return Engine::IsRunning(false) ? 1 : 0;
	}

	void bones_log(int level, const char* message)
	{
		if (message == nullptr)
		{
			return;
		}

		switch (static_cast<BonesLogLevel>(level))
		{
		case BONES_LOG_WARNING:
			Debug::PrintWarning(message);
			break;
		case BONES_LOG_ERROR:
			Debug::PrintError(message);
			break;
		case BONES_LOG_INFO:
		default:
			Debug::Print(message);
			break;
		}
	}

} // extern "C"
