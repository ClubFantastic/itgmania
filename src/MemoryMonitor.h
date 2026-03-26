// MemoryMonitor - periodic logging of process RSS, Lua heap, and malloc stats.
//
// Call MemoryMonitor::LogStats() once per frame (or every N frames).
// Enable at build time: -DWITH_MEMORY_MONITOR=ON (CMake) or
// define MEMORY_MONITOR before including this header.

#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

#if defined(MEMORY_MONITOR)

#include "RageLog.h"
#include "RageSurface.h"
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

#if defined(__linux__)
		// Periodically ask glibc to return freed memory to the OS.
		// Without this, malloc's arena fragments over time — freed blocks
		// stay resident as anonymous pages, causing RSS to grow even though
		// the application isn't actually leaking.
		malloc_trim(0);
#endif

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

		RageSurface_LogLeakStats();

		// Every 30 seconds, dump memory region and malloc info
		static int dumpCounter = 0;
		if (++dumpCounter % 30 == 0) {
			DumpAnonRegions();
			DumpMallocInfo();
		}
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

	static void DumpAnonRegions()
	{
#if defined(__linux__)
		FILE *f = fopen("/proc/self/smaps_rollup", "r");
		if (!f) return;
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			// Strip newline
			char *nl = strchr(line, '\n');
			if (nl) *nl = '\0';
			// Log interesting lines
			if (strstr(line, "Rss:") || strstr(line, "Anonymous:") ||
				strstr(line, "Shared") || strstr(line, "Private"))
				LOG->Info("MemMap: %s", line);
		}
		fclose(f);

		// Also count nvidia driver maps
		f = fopen("/proc/self/maps", "r");
		if (!f) return;
		int nvidiaCount = 0, totalAnon = 0;
		while (fgets(line, sizeof(line), f)) {
			if (strstr(line, "nvidia")) nvidiaCount++;
			// Count anonymous rw regions (no pathname, rw-p)
			if (strstr(line, "rw-p") && !strstr(line, "/") && !strstr(line, "["))
				totalAnon++;
		}
		fclose(f);
		LOG->Info("MemMap: nvidia_maps=%d anon_rw_regions=%d", nvidiaCount, totalAnon);
#endif
	}

	static void DumpMallocInfo()
	{
#if defined(__linux__)
		// malloc_info() gives per-arena details that mallinfo2() hides
		FILE *f = fopen("/tmp/itg_malloc_info.xml", "w");
		if (f) {
			malloc_info(0, f);
			fclose(f);
		}
		// Parse just the summary: total system bytes and in-use bytes
		f = fopen("/tmp/itg_malloc_info.xml", "r");
		if (!f) return;
		long totalSystem = 0, totalInUse = 0;
		int arenaCount = 0;
		char line[512];
		while (fgets(line, sizeof(line), f)) {
			long val;
			if (strstr(line, "<system type=\"current\"") && sscanf(strstr(line, "size=\""), "size=\"%ld\"", &val) == 1)
				totalSystem += val;
			if (strstr(line, "<total type=\"fast\"") && sscanf(strstr(line, "size=\""), "size=\"%ld\"", &val) == 1)
				totalInUse += val;
			if (strstr(line, "<total type=\"rest\"") && sscanf(strstr(line, "size=\""), "size=\"%ld\"", &val) == 1)
				totalInUse += val;
			if (strstr(line, "<total type=\"mmap\"") && sscanf(strstr(line, "size=\""), "size=\"%ld\"", &val) == 1)
				totalInUse += val;
			if (strstr(line, "<heap nr="))
				arenaCount++;
		}
		fclose(f);
		LOG->Info("MallocInfo: arenas=%d system=%ldMB inuse=%ldMB",
			arenaCount, totalSystem / (1024*1024), totalInUse / (1024*1024));
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
