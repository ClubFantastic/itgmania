#include "global.h"
#include "RageSoundManager.h"
#include "RageSounds.h"
#include "RageSound.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "GameState.h"
#include "TimingData.h"
#include "MsdFile.h"
#include "NotesLoaderSM.h"

RageSounds *SOUND = NULL;

/*
 * When playing music, automatically search for an SM file for timing data.  If one is
 * found, automatically handle GAMESTATE->m_fSongBeat, etc.
 *
 * modf(GAMESTATE->m_fSongBeat) should always be continuously moving from 0 to 1.  To do
 * this, wait before starting a sound until the fractional portion of the beat will be
 * the same.
 *
 * If PlayMusic(length_sec) is set, peek at the beat, and extend the length so we'll be
 * on the same fractional beat when we loop.  (XXX: should we increase fade_len, too?
 * That would cause the extra pad time to be silence.)
 */
/* Lock this before touching any of these globals. */
static RageMutex *g_Mutex;
static bool g_UpdatingTimer;
static bool g_ThreadedMusicStart = true;
static bool g_Shutdown;

struct MusicPlaying
{
	bool m_TimingDelayed;
	bool m_HasTiming;
	/* The timing data that we're currently using. */
	TimingData m_Timing;

	/* If m_TimingDelayed is true, this will be the timing data for the song that's starting.
	 * We'll copy it to m_Timing once sound is heard. */
	TimingData m_NewTiming;
	RageSound m_Music;
	MusicPlaying()
	{
		m_Timing.AddBPMSegment( BPMSegment(0,120) );
		m_NewTiming.AddBPMSegment( BPMSegment(0,120) );
		m_HasTiming = false;
		m_TimingDelayed = false;
	}
};

struct MusicToPlay
{
	CString file, timing_file;
	bool HasTiming;
	bool force_loop;
	float start_sec, length_sec, fade_len;
};

static MusicToPlay g_MusicToPlay;
static MusicPlaying *g_Playing;
static RageThread MusicThread;


void StartPlayingQueuedMusic( const RageTimer &when, const MusicToPlay &ToPlay, MusicPlaying &Playing )
{
	Playing.m_HasTiming = ToPlay.HasTiming;
	Playing.m_TimingDelayed = true;

	Playing.m_Music.Load( ToPlay.file, false );

	if( ToPlay.force_loop )
		Playing.m_Music.SetStopMode( RageSound::M_LOOP );

	Playing.m_Music.SetStartSeconds( ToPlay.start_sec );

	if( ToPlay.length_sec == -1 )
		Playing.m_Music.SetLengthSeconds();
	else
		Playing.m_Music.SetLengthSeconds( ToPlay.length_sec );

	Playing.m_Music.SetFadeLength( ToPlay.fade_len );
	Playing.m_Music.SetPositionSeconds();
	Playing.m_Music.SetStartTime( when );
	Playing.m_Music.StartPlaying();
}

void StartQueuedMusic( MusicToPlay &ToPlay )
{
	if( ToPlay.file.empty() )
		return;

	LockMutex L( *g_Mutex );
	MusicPlaying *NewMusic = new MusicPlaying;
	NewMusic->m_Timing = g_Playing->m_Timing;

	/* See if we can find timing data. */
	ToPlay.HasTiming = false;

	if( IsAFile(ToPlay.timing_file) )
	{
		LOG->Trace("Found '%s'", ToPlay.timing_file.c_str());
		MsdFile msd;
		bool bResult = msd.ReadFile( ToPlay.timing_file );
		if( !bResult )
			LOG->Warn( "Couldn't load %s, \"%s\"", ToPlay.timing_file.c_str(), msd.GetError().c_str() );
		else
		{
			SMLoader::LoadTimingFromSMFile( msd, NewMusic->m_NewTiming );
			ToPlay.HasTiming = true;
		}
	}

	if( ToPlay.HasTiming && ToPlay.force_loop && ToPlay.length_sec != -1 )
	{
		float fStartBeat = NewMusic->m_NewTiming.GetBeatFromElapsedTime( ToPlay.start_sec );
		float fEndSec = ToPlay.start_sec + ToPlay.length_sec;
		float fEndBeat = NewMusic->m_NewTiming.GetBeatFromElapsedTime( fEndSec );
		
		const float fStartBeatFraction = fmodfp( fStartBeat, 1 );
		const float fEndBeatFraction = fmodfp( fEndBeat, 1 );

		float fBeatDifference = fStartBeatFraction - fEndBeatFraction;
		if( fBeatDifference < 0 )
			fBeatDifference += 1.0f; /* unwrap */

		fEndBeat += fBeatDifference;

		float fRealEndSec = NewMusic->m_NewTiming.GetElapsedTimeFromBeat( fEndBeat );
		ToPlay.length_sec = fRealEndSec - ToPlay.start_sec;
	}

	bool StartImmediately = false;
	if( !ToPlay.HasTiming )
	{
		/* This song has no real timing data.  The offset is arbitrary.  Change it so
		 * the beat will line up to where we are now, so we don't have to delay. */
		float fDestBeat = fmodfp( GAMESTATE->m_fSongBeat, 1 );
		float fTime = NewMusic->m_NewTiming.GetElapsedTimeFromBeat( fDestBeat );

		NewMusic->m_NewTiming.m_fBeat0OffsetInSeconds = fTime;

		StartImmediately = true;
	}

	/* If we have an active timer, try to start on the next update.  Otherwise,
	 * start now. */
	if( !g_Playing->m_HasTiming && !g_UpdatingTimer )
		StartImmediately = true;

	RageTimer when; /* zero */
	if( !StartImmediately )
	{
		/* GetPlayLatency returns the minimum time until a sound starts.  That's
		 * common when starting a precached sound, but our sound isn't, so it'll
		 * probably take a little longer.  Nudge the latency up. */
		const float PresumedLatency = SOUND->GetPlayLatency() + 0.040f;
		const float fCurSecond = GAMESTATE->m_fMusicSeconds + PresumedLatency;
		const float fCurBeat = g_Playing->m_Timing.GetBeatFromElapsedTime( fCurSecond );
		const float fCurBeatFraction = fmodfp( fCurBeat,1 );

		/* The beat that the new sound will start on. */
		const float fStartBeat = NewMusic->m_NewTiming.GetBeatFromElapsedTime( ToPlay.start_sec );
		float fStartBeatFraction = fmodfp( fStartBeat, 1 );
		if( fStartBeatFraction < fCurBeatFraction )
			fStartBeatFraction += 1.0f; /* unwrap */

		const float fCurBeatToStartOn = truncf(fCurBeat) + fStartBeatFraction;
		const float fSecondToStartOn = g_Playing->m_Timing.GetElapsedTimeFromBeat( fCurBeatToStartOn );
		const float fDistance = fSecondToStartOn - fCurSecond;

		when = GAMESTATE->m_LastBeatUpdate + PresumedLatency + fDistance;
	}

	/* Important: don't hold the mutex while we load the actual sound. */
	L.Unlock();

	StartPlayingQueuedMusic( when, ToPlay, *NewMusic );

	LockMut( *g_Mutex );
	delete g_Playing;
	g_Playing = NewMusic;
}

int MusicThread_start( void *p )
{
	while( !g_Shutdown )
	{
		SDL_Delay( 10 );

		LockMutex L( *g_Mutex );
		if( !g_MusicToPlay.file.size() )
			continue;

		/* We have a sound to start.  Don't keep the lock while we do this; if another
		 * music tries to start in the meantime, it'll cause a skip. */
		MusicToPlay ToPlay = g_MusicToPlay;
		g_MusicToPlay.file = "";

		L.Unlock();
		StartQueuedMusic( ToPlay );
	}

	return 0;
}

RageSounds::RageSounds()
{
	/* Init RageSoundMan first: */
	ASSERT( SOUNDMAN );

	g_Mutex = new RageMutex;
	g_Playing = new MusicPlaying;

	g_UpdatingTimer = false;

	if( g_ThreadedMusicStart )
	{
		g_Shutdown = false;
		MusicThread.SetName( "MusicThread" );
		MusicThread.Create( MusicThread_start, this );
	}
}

RageSounds::~RageSounds()
{
	if( g_ThreadedMusicStart )
	{
		/* Signal the mixing thread to quit. */
		g_Shutdown = true;
		LOG->Trace("Shutting down music start thread ...");
		MusicThread.Wait();
		LOG->Trace("Music start thread shut down.");
	}

	delete g_Playing;
	delete g_Mutex;
}


void RageSounds::Update( float fDeltaTime )
{
	LockMut( *g_Mutex );

	if( !g_UpdatingTimer )
		return;

	if( !g_Playing->m_Music.IsPlaying() )
	{
		/* There's no song playing.  Fake it. */
		GAMESTATE->UpdateSongPosition( GAMESTATE->m_fMusicSeconds + fDeltaTime, g_Playing->m_Timing );
		return;
	}

	/* There's a delay between us calling Play() and the sound actually playing.
	 * During this time, approximate will be true.  Keep using the previous timing
	 * data until we get a non-approximate time, indicating that the sound has actually
	 * started playing. */
	bool approximate;
	const float fSeconds = g_Playing->m_Music.GetPositionSeconds( &approximate );

	if( g_Playing->m_TimingDelayed && !approximate )
	{
		/* We've passed the start position of the new sound, so we should be OK.
		 * Load up the new timing data. */
		g_Playing->m_Timing = g_Playing->m_NewTiming;
		g_Playing->m_TimingDelayed = false;
	}

	if( approximate )
	{
		/* We're still waiting for the new sound to start playing, so keep using the
		 * old timing data and fake the time. */
		GAMESTATE->UpdateSongPosition( GAMESTATE->m_fMusicSeconds + fDeltaTime, g_Playing->m_Timing );
		return;
	}

	GAMESTATE->UpdateSongPosition( fSeconds, g_Playing->m_Timing );
}


CString RageSounds::GetMusicPath() const
{
	LockMut( *g_Mutex );
	return g_Playing->m_Music.GetLoadedFilePath();
}

/* This function should not touch the disk at all. */
void RageSounds::PlayMusic( const CString &file, const CString &timing_file, bool force_loop, float start_sec, float length_sec, float fade_len )
{
	LockMut( *g_Mutex );
//	LOG->Trace("play '%s' (current '%s')", file.c_str(), g_Playing->m_Music.GetLoadedFilePath().c_str());
	if( g_Playing->m_Music.IsPlaying() )
	{
		if( !g_Playing->m_Music.GetLoadedFilePath().CompareNoCase(file) )
			return;		// do nothing

		g_Playing->m_Music.StopPlaying();
	}

	g_Playing->m_Music.Unload();

	MusicToPlay ToPlay;

	ToPlay.file = file;
	ToPlay.force_loop = force_loop;
	ToPlay.start_sec = start_sec;
	ToPlay.length_sec = length_sec;
	ToPlay.fade_len = fade_len;
	ToPlay.timing_file = timing_file;

	/* If no timing file was specified, look for one in the same place as the music file. */
	if( ToPlay.timing_file == "" )
		ToPlay.timing_file = SetExtension( file, "sm" );

	if( g_ThreadedMusicStart )
	{
		g_MusicToPlay = ToPlay;
		/* XXX: kick the music start thread */
	}
	else
		StartQueuedMusic( ToPlay );
}

void RageSounds::HandleSongTimer( bool on )
{
	LockMut( *g_Mutex );
	g_UpdatingTimer = on;
}

void RageSounds::PlayOnce( CString sPath )
{
	SOUNDMAN->PlayOnce( sPath );
}

void RageSounds::PlayOnceFromDir( CString PlayOnceFromDir )
{
	SOUNDMAN->PlayOnceFromDir( PlayOnceFromDir );
}

float RageSounds::GetPlayLatency() const
{
	return SOUNDMAN->GetPlayLatency();
}

/*
-----------------------------------------------------------------------------
 Copyright (c) 2002-2003 by the person(s) listed below.  All rights reserved.
        Glenn Maynard
-----------------------------------------------------------------------------
*/

