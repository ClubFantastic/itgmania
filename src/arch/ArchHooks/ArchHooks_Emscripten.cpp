#include "global.h"
#include "ArchHooks_Emscripten.h"
#include "RageFileManager.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "ProductInfo.h"

#include <cstdint>
#include <emscripten.h>

void ArchHooks_Emscripten::Init()
{
	/* No signal handlers or crash handlers in the browser. */
}

int64_t ArchHooks::GetSystemTimeInMicroseconds()
{
	/* emscripten_get_now() returns milliseconds as a double with
	 * sub-millisecond precision (uses performance.now() internally). */
	double ms = emscripten_get_now();
	return static_cast<int64_t>( ms * 1000.0 );
}

RString ArchHooks::GetPreferredLanguage()
{
	/* Could use EM_ASM to query navigator.language, but for now default to English. */
	return "en";
}

void ArchHooks_Emscripten::DumpDebugInfo()
{
	LOG->Info( "Platform: Emscripten/WebAssembly" );
}

void ArchHooks::MountInitialFilesystems( const RString &sDirOfExecutable )
{
	/* In the browser, all game data is preloaded into Emscripten's virtual
	 * filesystem at the executable directory. Mount it read-only at root. */
	FILEMAN->Mount( "dirro", sDirOfExecutable, "/" );
}

void ArchHooks::MountUserFilesystems( const RString &sDirOfExecutable )
{
	/* User data (Save/, Cache/, etc.) goes into the same virtual filesystem.
	 * In the future this can be backed by IndexedDB via IDBFS for persistence. */
	RString sUserDataPath = sDirOfExecutable;
	FILEMAN->Mount( "dir", sUserDataPath + "/Save", "/Save" );
	FILEMAN->Mount( "dir", sUserDataPath + "/Cache", "/Cache" );
	FILEMAN->Mount( "dir", sUserDataPath + "/Logs", "/Logs" );
	FILEMAN->Mount( "dir", sUserDataPath + "/Screenshots", "/Screenshots" );
}

/*
 * (c) 2025 ITGmania contributors
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
