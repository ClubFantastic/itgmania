/* RageFileDriverHTTP: Lazy HTTP file driver for Emscripten.
 * Bypasses FilenameDB entirely — uses a simple in-memory manifest
 * for directory listings and file lookups. */

#ifdef EMSCRIPTEN

#include "global.h"
#include "RageFileDriverHTTP.h"
#include "RageFile.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "RageUtil_FileDB.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <emscripten.h>

/* ---- RageFileObjHTTP ---- */

RageFileObjHTTP::RageFileObjHTTP( const std::string &sURL )
	: m_sURL(sURL), m_iFilePos(0), m_bFetched(false), m_bFetchFailed(false)
{
}

RageFileObjHTTP::~RageFileObjHTTP() {}

bool RageFileObjHTTP::EnsureFetched()
{
	if( m_bFetched )
		return !m_bFetchFailed;
	m_bFetched = true;

	void *pBuf = nullptr;
	int iSize = 0, iError = 0;
	emscripten_wget_data( m_sURL.c_str(), &pBuf, &iSize, &iError );

	if( iError != 0 || pBuf == nullptr )
	{
		if( LOG ) LOG->Warn( "HTTP: fetch failed '%s' (err %d)", m_sURL.c_str(), iError );
		m_bFetchFailed = true;
		if( pBuf ) free( pBuf );
		return false;
	}

	m_Data.assign( (char*)pBuf, (char*)pBuf + iSize );
	free( pBuf );
	return true;
}

int RageFileObjHTTP::ReadInternal( void *pBuffer, size_t iBytes )
{
	if( !EnsureFetched() ) return -1;
	int iRemain = (int)m_Data.size() - m_iFilePos;
	if( iRemain <= 0 ) return 0;
	int iToRead = std::min( (int)iBytes, iRemain );
	memcpy( pBuffer, m_Data.data() + m_iFilePos, iToRead );
	m_iFilePos += iToRead;
	return iToRead;
}

int RageFileObjHTTP::SeekInternal( int iOffset )
{
	if( !EnsureFetched() ) return 0;
	m_iFilePos = std::clamp( iOffset, 0, (int)m_Data.size() );
	return m_iFilePos;
}

int RageFileObjHTTP::GetFileSize() const
{
	const_cast<RageFileObjHTTP*>(this)->EnsureFetched();
	return m_bFetchFailed ? 0 : (int)m_Data.size();
}

RageFileObjHTTP *RageFileObjHTTP::Copy() const
{
	auto *p = new RageFileObjHTTP( m_sURL );
	p->m_Data = m_Data;
	p->m_iFilePos = m_iFilePos;
	p->m_bFetched = m_bFetched;
	p->m_bFetchFailed = m_bFetchFailed;
	return p;
}

/* ---- Path helpers ---- */

static std::string toLower( const std::string &s )
{
	std::string r = s;
	for( char &c : r )
		if( c >= 'A' && c <= 'Z' ) c += 'a' - 'A';
	return r;
}

/* Normalize a path: ensure leading /, lowercase, collapse slashes. */
std::string RageFileDriverHTTP::NormPath( const std::string &sPath ) const
{
	std::string s = sPath.c_str();
	/* Ensure leading slash */
	if( s.empty() || s[0] != '/' )
		s = "/" + s;
	/* Replace backslashes */
	for( char &c : s )
		if( c == '\\' ) c = '/';
	return toLower( s );
}

/* ---- RageFileDriverHTTP ---- */

RageFileDriverHTTP::RageFileDriverHTTP( const std::string &sBaseURL )
	: RageFileDriver( new NullFilenameDB ),
	  m_sBaseURL( sBaseURL )
{
	if( !m_sBaseURL.empty() && m_sBaseURL.Right(1) != "/" )
		m_sBaseURL += "/";
	LoadManifest();
}

void RageFileDriverHTTP::LoadManifest()
{
	std::string sManifestURL = m_sBaseURL + "gamedata-manifest.txt";
	void *pBuf = nullptr;
	int iSize = 0, iError = 0;
	emscripten_wget_data( sManifestURL.c_str(), &pBuf, &iSize, &iError );

	if( iError != 0 || pBuf == nullptr )
	{
		printf( "HTTP-VFS: failed to fetch manifest from '%s'\n", sManifestURL.c_str() );
		if( pBuf ) free( pBuf );
		return;
	}

	std::string sManifest( (const char*)pBuf, iSize );
	free( pBuf );

	std::vector<std::string> asLines;
	split( sManifest, "\n", asLines );

	int iFiles = 0, iDirs = 0;
	for( const std::string &sLine : asLines )
	{
		if( sLine.empty() ) continue;
		size_t iTab = sLine.find( '\t' );
		if( iTab == std::string::npos ) continue;

		int iFileSize = StringToInt( sLine.Left(iTab) );
		std::string sPath = sLine.substr( iTab + 1 );
		sPath.Replace( "\\", "/" );

		bool bIsDir = (iFileSize == -1);

		/* Build the normalized path with leading slash */
		std::string sOrigPath = sPath.c_str();
		if( sOrigPath.empty() ) continue;
		if( sOrigPath[0] != '/' )
			sOrigPath = "/" + sOrigPath;

		std::string sNorm = toLower( sOrigPath );

		/* Store in manifest */
		if( bIsDir )
		{
			std::string sDirNorm = sNorm;
			if( sDirNorm.back() != '/' ) sDirNorm += "/";
			m_Manifest[sDirNorm] = { true, 0 };
			iDirs++;
		}
		else
		{
			m_Manifest[sNorm] = { false, iFileSize };
			iFiles++;
		}

		/* Add to parent's directory listing */
		std::string sOrigWithSlash = sOrigPath;
		if( bIsDir && sOrigWithSlash.back() != '/' )
			sOrigWithSlash += "/";

		/* Find parent dir and child name */
		std::string sForSplit = bIsDir ?
			sOrigPath : sOrigPath; /* use without trailing slash for splitting */
		/* Remove trailing slash for splitting */
		std::string sTmp = sOrigPath;
		while( sTmp.size() > 1 && sTmp.back() == '/' )
			sTmp.pop_back();
		size_t iLastSlash = sTmp.rfind( '/' );
		if( iLastSlash == std::string::npos ) continue;

		std::string sParentDir = toLower( sTmp.substr( 0, iLastSlash + 1 ) );
		std::string sChildName = sTmp.substr( iLastSlash + 1 );

		m_DirContents[sParentDir].push_back( { sChildName, bIsDir } );
	}

	/* Also add the root directory */
	m_Manifest["/"] = { true, 0 };

	printf( "HTTP-VFS: loaded manifest: %d files, %d dirs\n", iFiles, iDirs );
}

RageFileBasic *RageFileDriverHTTP::Open( const std::string &sPath, int iMode, int &iError )
{
	if( iMode != RageFile::READ )
	{
		iError = ERROR_WRITING_NOT_SUPPORTED;
		return nullptr;
	}

	std::string sNorm = NormPath( sPath );

	auto it = m_Manifest.find( sNorm );
	if( it == m_Manifest.end() || it->second.bIsDir )
	{
		iError = (it != m_Manifest.end()) ? EISDIR : ENOENT;
		return nullptr;
	}

	/* Build URL */
	std::string sRelPath = sPath;
	if( !sRelPath.empty() && sRelPath[0] == '/' )
		sRelPath = sRelPath.substr(1);
	return new RageFileObjHTTP( m_sBaseURL + sRelPath );
}

void RageFileDriverHTTP::GetDirListing( const std::string &sPath,
	std::vector<std::string> &asAddTo, bool bOnlyDirs, bool bReturnPathToo )
{
	/* sPath is like "/Themes/_fallback/Graphics/_missing*" or "/Songs/*" */
	std::string s = sPath.c_str();
	if( s.empty() ) return;
	if( s[0] != '/' ) s = "/" + s;

	/* Split into directory part and filename pattern */
	size_t iLastSlash = s.rfind( '/' );
	std::string sDirPart = toLower( s.substr( 0, iLastSlash + 1 ) );
	std::string sPattern = s.substr( iLastSlash + 1 );

	/* Check for wildcard */
	size_t iStar = sPattern.find( '*' );

	auto dirIt = m_DirContents.find( sDirPart );
	if( dirIt == m_DirContents.end() )
		return;

	std::string sPatLower = toLower( sPattern );

	for( auto &entry : dirIt->second )
	{
		const std::string &sName = entry.first;
		bool bIsDir = entry.second;

		if( bOnlyDirs && !bIsDir )
			continue;

		/* Match against pattern */
		std::string sNameLower = toLower( sName );

		if( iStar == std::string::npos )
		{
			/* Exact match */
			if( sNameLower != sPatLower )
				continue;
		}
		else
		{
			/* Wildcard: "prefix*suffix" */
			std::string sPrefix = toLower( sPattern.substr( 0, iStar ) );
			std::string sSuffix = toLower( sPattern.substr( iStar + 1 ) );

			if( !sPrefix.empty() && sNameLower.substr( 0, sPrefix.size() ) != sPrefix )
				continue;
			if( !sSuffix.empty() )
			{
				if( sNameLower.size() < sSuffix.size() )
					continue;
				if( sNameLower.substr( sNameLower.size() - sSuffix.size() ) != sSuffix )
					continue;
			}
		}

		if( bReturnPathToo )
		{
			/* Reconstruct the directory part from the original path (preserve case) */
			std::string sOrigDir = s.substr( 0, iLastSlash + 1 );
			asAddTo.push_back( sOrigDir + sName + (bIsDir ? "/" : "") );
		}
		else
		{
			asAddTo.push_back( sName + (bIsDir ? "/" : "") );
		}
	}
}

RageFileManager::FileType RageFileDriverHTTP::GetFileType( const std::string &sPath )
{
	std::string sNorm = NormPath( sPath );

	/* Try as file first */
	auto it = m_Manifest.find( sNorm );
	if( it != m_Manifest.end() )
		return it->second.bIsDir ? RageFileManager::TYPE_DIR : RageFileManager::TYPE_FILE;

	/* Try with trailing slash (directory) */
	if( sNorm.back() != '/' )
	{
		it = m_Manifest.find( sNorm + "/" );
		if( it != m_Manifest.end() )
			return RageFileManager::TYPE_DIR;
	}

	return RageFileManager::TYPE_NONE;
}

int RageFileDriverHTTP::GetFileSizeInBytes( const std::string &sFilePath )
{
	std::string sNorm = NormPath( sFilePath );
	auto it = m_Manifest.find( sNorm );
	if( it == m_Manifest.end() || it->second.bIsDir )
		return -1;
	return it->second.iSize;
}

/* Register driver type "HTTP" */
static struct FileDriverEntry_HTTP: public FileDriverEntry
{
	FileDriverEntry_HTTP(): FileDriverEntry( "HTTP" ) { }
	RageFileDriver *Create( const std::string &sRoot ) const
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
