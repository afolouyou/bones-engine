// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Xenity Engine

#include "time.h"

#include <chrono>

#include <engine/tools/scope_benchmark.h>
#include <engine/debug/performance.h>
#include <engine/debug/debug.h>
#include <engine/debug/stack_debug_object.h>

float Time::s_timeScale = 1;
float Time::s_time = 0;
float Time::s_unscaledTime = 0;
float Time::s_deltaTime = 0;
float Time::s_unscaledDeltaTime = 0;

std::chrono::time_point<std::chrono::high_resolution_clock> start_point, end_point;

#pragma region Accessors

void Time::SetTimeScale(float _timeScale)
{
	STACK_DEBUG_OBJECT(STACK_MEDIUM_PRIORITY);
	if (_timeScale < 0)
		_timeScale = 0;

	Time::s_timeScale = _timeScale;
}

#pragma endregion

void Time::Init()
{
	STACK_DEBUG_OBJECT(STACK_HIGH_PRIORITY);
	Reset();
	Debug::Print("-------- Time system initiated --------", true);
}

void Time::Reset()
{
	start_point = std::chrono::high_resolution_clock::now();
	end_point = start_point;
	s_time = 0;
	s_unscaledTime = 0;
}

void Time::UpdateTime()
{
	STACK_DEBUG_OBJECT(STACK_HIGH_PRIORITY);

	SCOPED_PROFILER("Time::UpdateTime", scopeBenchmark);
	start_point = std::chrono::high_resolution_clock::now();
	const float tempDeltaTime = std::chrono::duration<float>(start_point - end_point).count();
	end_point = start_point;
	s_deltaTime = tempDeltaTime * s_timeScale;
	s_unscaledDeltaTime = tempDeltaTime;
	
	if (s_deltaTime >= 0.2f) 
	{
		s_deltaTime = 0.2f;
	}

	if (s_unscaledDeltaTime >= 0.2f)
	{
		s_unscaledDeltaTime = 0.2f;
	}

	s_time += s_deltaTime;
	s_unscaledTime += s_unscaledDeltaTime;
}