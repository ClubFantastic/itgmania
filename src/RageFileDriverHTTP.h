/* RageFileDriverHTTP: Lazy HTTP file driver for Emscripten.
 * Files are fetched on demand from a web server using synchronous XHR.
 * A JSON manifest provides the directory structure. */

#ifndef RAGE_FILE_DRIVER_HTTP_H
#define RAGE_FILE_DRIVER_HTTP_H

#ifdef EMSCRIPTEN

#include "RageFileDriver.h"
#include "RageFileBasic.h"

#include <cstddef>
#include <string>
#include <vector>

class RageFileObjHTTP: public RageFileObj
{
public:
	RageFileObjHTTP( const std::string &sURL );
	~RageFileObjHTTP();

	int ReadInternal( void *pBuffer, size_t iBytes );
	int WriteInternal( const void * /*pBuffer*/, size_t /*iBytes*/ ) { return -1; }
	int FlushInternal() { return 0; }
	int SeekInternal( int iOffset );
	int GetFileSize() const;
	RageFileObjHTTP *Copy() const;
	RString GetDisplayPath() const { return m_sURL; }

private:
	bool EnsureFetched();

	std::string m_sURL;
	std::vector<char> m_Data;
	int m_iFilePos;
	bool m_bFetched;
	bool m_bFetchFailed;
};

class RageFileDriverHTTP: public RageFileDriver
{
public:
	RageFileDriverHTTP( const RString &sBaseURL );

	RageFileBasic *Open( const RString &sPath, int iMode, int &iError );
	void FlushDirCache( const RString &sPath ) override {
		printf("HTTP-VFS: FlushDirCache('%s') BLOCKED\n", sPath.c_str());
	}

private:
	RString m_sBaseURL;

	void LoadManifest();
};

#endif // EMSCRIPTEN
#endif // RAGE_FILE_DRIVER_HTTP_H

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
