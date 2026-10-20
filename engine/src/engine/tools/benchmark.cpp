// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "benchmark.h"


/// <summary>
/// Start the benchmark timer
/// </summary>
void Benchmark::Start()
{
#if defined(_WIN32) || defined(_WIN64) || defined(__LINUX__)
	m_start_point = std::chrono::high_resolution_clock::now();
#endif
}

/// <summary>
/// Stop the benchmark timer
/// </summary>
void Benchmark::Stop()
{
#if defined(_WIN32) || defined(_WIN64) || defined(__LINUX__)
	m_end_point = std::chrono::high_resolution_clock::now();

	const int64_t start = std::chrono::time_point_cast<std::chrono::microseconds>(m_start_point).time_since_epoch().count();
	const int64_t end = std::chrono::time_point_cast<std::chrono::microseconds>(m_end_point).time_since_epoch().count();

	m_time = end - start;
#endif
}