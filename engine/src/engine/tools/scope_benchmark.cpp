#include "scope_benchmark.h"

#include <chrono>

#include <engine/debug/performance.h>
#include <engine/debug/debug.h>

#if defined(__PS3__)
#include <ppu-asm.h>
#include <sys/systime.h>
#endif

ScopeBenchmark::ScopeBenchmark(const size_t hash) : m_hash(hash), m_scopeLevel(Performance::s_benchmarkScopeLevel)
{
#if defined(__PS3__)
	static uint64_t freg = sysGetTimebaseFrequency();
	m_start = (__gettime() / (double)freg) * 1000000;
#else
	m_start = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
#endif
	Performance::s_benchmarkScopeLevel++;
}

ScopeBenchmark::~ScopeBenchmark()
{
	uint64_t end;
#if defined(__PS3__)
	static uint64_t freg = sysGetTimebaseFrequency();
	end = (__gettime() / (double)freg) * 1000000;
#else
	end = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
#endif
	Performance::s_benchmarkScopeLevel--;
	if (!Performance::s_isPaused) 
	{
		Performance::s_scopProfilerList[Performance::s_currentProfilerFrame].timerResults[m_hash].push_back({ m_start , end , m_scopeLevel });
	}
}
