#include "global.h"
#include "MemoryMonitor.h"

#if defined(MEMORY_MONITOR)

#include "LuaManager.h"

long MemoryMonitor::GetLuaKB()
{
	if (!LUA) return 0;
	Lua* L = LUA->Get();
	if (!L) return 0;
	int kb = lua_gc(L, LUA_GCCOUNT, 0);
	LUA->Release(L);
	return (long)kb;
}

#endif // MEMORY_MONITOR
