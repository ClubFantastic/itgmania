/* RageFileDriverHTTP: Lazy HTTP file driver for Emscripten.
 * Files are fetched on demand from a web server via synchronous XHR.
 * A JSON manifest (gamedata-manifest.json) provides file listings. */

#ifdef EMSCRIPTEN

#include "global.h"
#include "RageFileDriverHTTP.h"
#include "RageFile.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "RageUtil_FileDB.h"
#include "JsonUtil.h"

#include <cerrno>
#include <emscripten.h>

/* Custom FilenameDB that never expires and never creates empty FileSets
 * for directories that aren't in the manifest. */
class HTTPFilenameDB: public FilenameDB
{
public:
	HTTPFilenameDB() { ExpireSeconds = -1; }
	void CacheFile( const RString & ) override { }
	void PopulateFileSet( FileSet &fs, const RString &sPath ) override
	{
		/* This should never be called for paths in the manifest, since
		 * AddFile already created the FileSets.  If it IS called, it means
		 * GetFileSet is creating a new empty FileSet for a path that
		 * doesn't exist in the manifest — which is fine, just leave it empty. */
	}
};

/* ---- RageFileObjHTTP ---- */

RageFileObjHTTP::RageFileObjHTTP( const std::string &sURL )
	: m_sURL(sURL), m_iFilePos(0), m_bFetched(false), m_bFetchFailed(false)
{
}

RageFileObjHTTP::~RageFileObjHTTP()
{
}

bool RageFileObjHTTP::EnsureFetched()
{
	if( m_bFetched )
		return !m_bFetchFailed;

	m_bFetched = true;

	void *pBuf = nullptr;
	int iSize = 0;
	int iError = 0;

	/* Synchronous HTTP GET. This blocks the main thread, which is acceptable
	 * for a game that does synchronous file I/O everywhere. */
	emscripten_wget_data( m_sURL.c_str(), &pBuf, &iSize, &iError );

	if( iError != 0 || pBuf == nullptr )
	{
		if( LOG ) LOG->Warn( "RageFileDriverHTTP: failed to fetch '%s' (error %d)", m_sURL.c_str(), iError );
		m_bFetchFailed = true;
		if( pBuf )
			free( pBuf );
		return false;
	}

	m_Data.assign( (char*)pBuf, (char*)pBuf + iSize );
	free( pBuf );
	return true;
}

int RageFileObjHTTP::ReadInternal( void *pBuffer, size_t iBytes )
{
	if( !EnsureFetched() )
		return -1;

	int iRemain = (int)m_Data.size() - m_iFilePos;
	if( iRemain <= 0 )
		return 0;

	int iToRead = std::min( (int)iBytes, iRemain );
	memcpy( pBuffer, m_Data.data() + m_iFilePos, iToRead );
	m_iFilePos += iToRead;
	return iToRead;
}

int RageFileObjHTTP::SeekInternal( int iOffset )
{
	if( !EnsureFetched() )
		return 0;

	m_iFilePos = std::clamp( iOffset, 0, (int)m_Data.size() );
	return m_iFilePos;
}

int RageFileObjHTTP::GetFileSize() const
{
	/* Must eagerly fetch — callers (RageFileObj::Read) use GetFileSize()
	 * to pre-allocate buffers, and -1 would be interpreted as a huge size. */
	const_cast<RageFileObjHTTP*>(this)->EnsureFetched();
	if( m_bFetchFailed )
		return 0;
	return (int)m_Data.size();
}

RageFileObjHTTP *RageFileObjHTTP::Copy() const
{
	auto *pCopy = new RageFileObjHTTP( m_sURL );
	pCopy->m_Data = m_Data;
	pCopy->m_iFilePos = m_iFilePos;
	pCopy->m_bFetched = m_bFetched;
	pCopy->m_bFetchFailed = m_bFetchFailed;
	return pCopy;
}

/* ---- RageFileDriverHTTP ---- */

RageFileDriverHTTP::RageFileDriverHTTP( const RString &sBaseURL )
	: RageFileDriver( new HTTPFilenameDB ),
	  m_sBaseURL( sBaseURL )
{
	/* Ensure base URL has trailing slash */
	if( !m_sBaseURL.empty() && m_sBaseURL.Right(1) != "/" )
		m_sBaseURL += "/";

	LoadManifest();
}

void RageFileDriverHTTP::LoadManifest()
{
	/* Fetch the manifest file, which is a newline-delimited list of:
	 *   size<TAB>path
	 * Directories are indicated by size -1. */
	RString sManifestURL = m_sBaseURL + "gamedata-manifest.txt";

	void *pBuf = nullptr;
	int iSize = 0;
	int iError = 0;

	emscripten_wget_data( sManifestURL.c_str(), &pBuf, &iSize, &iError );

	if( iError != 0 || pBuf == nullptr )
	{
		if( LOG ) LOG->Warn( "RageFileDriverHTTP: failed to fetch manifest from '%s'", sManifestURL.c_str() );
		if( pBuf )
			free( pBuf );
		return;
	}

	RString sManifest( (const char*)pBuf, iSize );
	free( pBuf );

	std::vector<RString> asLines;
	split( sManifest, "\n", asLines );

	for( const RString &sLine : asLines )
	{
		if( sLine.empty() )
			continue;

		/* Format: size\tpath */
		size_t iTab = sLine.find( '\t' );
		if( iTab == RString::npos )
			continue;

		int iFileSize = StringToInt( sLine.Left(iTab) );
		RString sPath = sLine.substr( iTab + 1 );

		/* Normalize path separators */
		sPath.Replace( "\\", "/" );

		/* Ensure leading slash */
		if( sPath.empty() )
			continue;
		if( sPath[0] != '/' )
			sPath = "/" + sPath;

		/* Directories (size -1) need trailing slash for FilenameDB */
		if( iFileSize == -1 )
		{
			if( sPath.Right(1) != "/" )
				sPath += "/";
		}

		FDB->AddFile( sPath, iFileSize, 0, nullptr );
	}

	int iFiles = 0, iDirs = 0;
	for( const RString &sLine : asLines )
	{
		if( sLine.empty() ) continue;
		size_t t = sLine.find('\t');
		if( t == RString::npos ) continue;
		if( sLine.Left(t) == "-1" ) iDirs++; else iFiles++;
	}
	if( LOG ) LOG->Info( "RageFileDriverHTTP: loaded manifest with %d files and %d dirs from '%s'",
		iFiles, iDirs, sManifestURL.c_str() );

	/* Debug: verify key files are findable */
	{
		RageFileManager::FileType ft;
		ft = FDB->GetFileType( "/Themes/_fallback/Graphics/_missing.png" );
		printf( "HTTP-VFS: _missing.png type = %d\n", (int)ft );
		ft = FDB->GetFileType( "/Themes/_fallback/Graphics/" );
		printf( "HTTP-VFS: Graphics/ dir type = %d\n", (int)ft );
		ft = FDB->GetFileType( "/Themes/" );
		printf( "HTTP-VFS: Themes/ dir type = %d\n", (int)ft );
		ft = FDB->GetFileType( "/Characters/default/" );
		printf( "HTTP-VFS: Characters/default/ type = %d\n", (int)ft );

		std::vector<RString> listing;
		FDB->GetDirListing( "/Themes/_fallback/Graphics/_missing*", listing, false, false );
		printf( "HTTP-VFS: _missing* listing has %d results\n", (int)listing.size() );
		for( size_t i = 0; i < listing.size(); i++ )
			printf( "HTTP-VFS:   [%d] %s\n", (int)i, listing[i].c_str() );
	}
}

RageFileBasic *RageFileDriverHTTP::Open( const RString &sPath, int iMode, int &iError )
{
	if( iMode != RageFile::READ )
	{
		iError = ERROR_WRITING_NOT_SUPPORTED;
		return nullptr;
	}

	/* Check that the file exists in our manifest */
	if( FDB->GetFileType(sPath) == RageFileManager::TYPE_NONE )
	{
		iError = ENOENT;
		return nullptr;
	}

	if( FDB->GetFileType(sPath) == RageFileManager::TYPE_DIR )
	{
		iError = EISDIR;
		return nullptr;
	}

	/* Build the URL: base + path (strip leading slash since base has trailing slash) */
	RString sRelPath = sPath;
	if( !sRelPath.empty() && sRelPath[0] == '/' )
		sRelPath = sRelPath.substr(1);

	RString sURL = m_sBaseURL + sRelPath;

	return new RageFileObjHTTP( sURL );
}

/* Register driver type "HTTP" */
static struct FileDriverEntry_HTTP: public FileDriverEntry
{
	FileDriverEntry_HTTP(): FileDriverEntry( "HTTP" ) { }
	RageFileDriver *Create( const RString &sRoot ) const
	{
		return new RageFileDriverHTTP( sRoot );
	}
} const g_RegisterDriver;

#endif // EMSCRIPTEN

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
