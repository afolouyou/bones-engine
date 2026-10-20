#include "memory_info.h"

#include <string>
#include <engine/debug/debug.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <psapi.h>
#include <dxgi1_4.h>
#include <processthreadsapi.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
IDXGIAdapter3* adapter;
#endif

void MemoryInfo::Init()
{
#if defined(_WIN32) || defined(_WIN64)
	IDXGIFactory4* pFactory;
	CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&pFactory);
	pFactory->EnumAdapters(0, reinterpret_cast<IDXGIAdapter**>(&adapter));
#endif

}

size_t MemoryInfo::GetUsedMemory()
{
#if defined(_WIN32) || defined(_WIN64)
	PROCESS_MEMORY_COUNTERS s_memoryInfo;
	GetProcessMemoryInfo(GetCurrentProcess(), &s_memoryInfo, sizeof(s_memoryInfo));
	return s_memoryInfo.WorkingSetSize;
#else
	return 0;
#endif
}

size_t MemoryInfo::GetTotalMemory()
{
#if defined(_WIN32) || defined(_WIN64)
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullTotalPhys;
#else
	return 0;
#endif
}

size_t MemoryInfo::GetUsedVideoMemory()
{
#if defined(_WIN32) || defined(_WIN64)
	DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
	adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo);
	size_t usedVRAM = videoMemoryInfo.CurrentUsage;
	return usedVRAM;
#else
	return 0;
#endif
}

size_t MemoryInfo::GetTotalVideoMemory()
{
#if defined(_WIN32) || defined(_WIN64)
	DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
	adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo);
	size_t maxVRAM = videoMemoryInfo.Budget;
	return maxVRAM;
#else
	return 0;
#endif
}
