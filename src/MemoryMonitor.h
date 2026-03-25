// MemoryMonitor - periodic logging of process RSS, Lua heap, and malloc stats.
//
// Call MemoryMonitor::LogStats() once per frame (or every N frames).
// Enable at build time: -DWITH_MEMORY_MONITOR=ON (CMake) or
// define MEMORY_MONITOR before including this header.

#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

#if defined(MEMORY_MONITOR)

#include "RageLog.h"
#include "RageDisplay_OGL_Helpers.h"

#include <cstdio>

#if defined(__linux__)
#include <malloc.h>  // mallinfo2
#endif

class MemoryMonitor
{
public:
	// Log memory stats every N calls. Delta-only by default.
	static void LogStats(int everyNFrames = 300)
	{
		static int frameCounter = 0;
		++frameCounter;
		if (frameCounter % everyNFrames != 0)
			return;

		long rssKB = GetRSSKB();
		long luaKB = GetLuaKB();

		static long prevRSS = 0;
		static long prevLua = 0;
		static long prevArena = 0;
		static long prevMmap = 0;

		long arenaKB = 0, mmapKB = 0;
#if defined(__linux__)
		struct mallinfo2 mi = mallinfo2();
		arenaKB = (long)(mi.arena / 1024);
		mmapKB = (long)(mi.hblkhd / 1024);
#endif

		long dRSS = rssKB - prevRSS;
		long dLua = luaKB - prevLua;
		long dArena = arenaKB - prevArena;
		long dMmap = mmapKB - prevMmap;

		long gpuAvailKB = GetGPUAvailableKB();
		long anonKB = GetAnonKB();

		static long prevGPU = 0;
		static long prevAnon = 0;

		long dGPU = gpuAvailKB - prevGPU;
		long dAnon = anonKB - prevAnon;

		LOG->Info("MemMon [frame %d]: RSS=%ldMB(%+ldKB) Lua=%ldKB(%+ldKB) "
			"malloc_arena=%ldKB(%+ldKB) malloc_mmap=%ldKB(%+ldKB) "
			"anon=%ldMB(%+ldKB) gpu_avail=%ldMB(%+ldKB)",
			frameCounter,
			rssKB / 1024, dRSS,
			luaKB, dLua,
			arenaKB, dArena,
			mmapKB, dMmap,
			anonKB / 1024, dAnon,
			gpuAvailKB / 1024, dGPU);

		prevRSS = rssKB;
		prevLua = luaKB;
		prevArena = arenaKB;
		prevMmap = mmapKB;
		prevGPU = gpuAvailKB;
		prevAnon = anonKB;
	}

	// Force-log regardless of frame counter
	static void DumpStats()
	{
		LogStats(1);
	}

private:
	static long GetRSSKB()
	{
#if defined(__linux__)
		FILE* f = fopen("/proc/self/statm", "r");
		if (!f) return 0;
		long pages = 0, resident = 0;
		if (fscanf(f, "%ld %ld", &pages, &resident) != 2)
			resident = 0;
		fclose(f);
		// resident is in pages, typically 4KB
		return resident * 4;
#else
		return 0;
#endif
	}

	static long GetLuaKB();  // defined in MemoryMonitor.cpp to avoid including LuaManager.h

	static long GetGPUAvailableKB()
	{
#ifndef GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049
#endif
		GLint availKB = 0;
		glGetIntegerv(GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, &availKB);
		// Returns 0 if the extension isn't available
		return (long)availKB;
	}

	static long GetAnonKB()
	{
#if defined(__linux__)
		// Read RssAnon from /proc/self/status — this is the anonymous
		// (heap + mmap'd non-file) resident memory, excluding file-backed pages
		FILE* f = fopen("/proc/self/status", "r");
		if (!f) return 0;
		char line[256];
		long anonKB = 0;
		while (fgets(line, sizeof(line), f)) {
			if (sscanf(line, "RssAnon: %ld kB", &anonKB) == 1)
				break;
		}
		fclose(f);
		return anonKB;
#else
		return 0;
#endif
	}
};

#define MEM_MON_LOG_STATS()   MemoryMonitor::LogStats()
#define MEM_MON_DUMP_STATS()  MemoryMonitor::DumpStats()

#else // MEMORY_MONITOR not defined

#define MEM_MON_LOG_STATS()   ((void)0)
#define MEM_MON_DUMP_STATS()  ((void)0)

#endif // MEMORY_MONITOR
#endif // MEMORY_MONITOR_H
