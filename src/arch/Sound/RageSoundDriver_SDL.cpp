#include "global.h"
#include "RageSoundDriver_SDL.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "RageTimer.h"
#include "PrefsManager.h"
#include "RageSoundConstants.h"

#include <cstdint>
#include <cstring>
#include <SDL3/SDL.h>

REGISTER_SOUND_DRIVER_CLASS2( SDL, SDL );

static const int channels = 2;

RageSoundDriver_SDL::RageSoundDriver_SDL() :
	RageSoundDriver()
{
	m_pStream = nullptr;
	m_iSampleRate = 0;
	m_iWriteahead = 0;
	m_iFramesWritten = 0;
}

RageSoundDriver_SDL::~RageSoundDriver_SDL()
{
	if( m_pStream != nullptr )
	{
		SDL_PauseAudioStreamDevice( m_pStream );
		SDL_DestroyAudioStream( m_pStream );
	}
}

std::string RageSoundDriver_SDL::Init()
{
	if( !SDL_WasInit(SDL_INIT_AUDIO) )
	{
		if( !SDL_InitSubSystem(SDL_INIT_AUDIO) )
			return ssprintf( "SDL_InitSubSystem(AUDIO) failed: %s", SDL_GetError() );
	}

	m_iSampleRate = PREFSMAN->m_iSoundPreferredSampleRate;
	if( m_iSampleRate == 0 )
		m_iSampleRate = FALLBACK_SAMPLE_RATE;

	// On Emscripten, query the device's native sample rate first and use it
	// directly to avoid resampling artifacts in the Web Audio API.
#ifdef EMSCRIPTEN
	{
		SDL_AudioSpec devSpec;
		int devFrames = 0;
		if( SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &devSpec, &devFrames) && devSpec.freq > 0 )
			m_iSampleRate = devSpec.freq;
	}
#endif

	SDL_AudioSpec spec;
	spec.format = SDL_AUDIO_S16;
	spec.channels = channels;
	spec.freq = m_iSampleRate;

	m_pStream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		&spec, AudioCallback, this );

	if( m_pStream == nullptr )
		return ssprintf( "SDL_OpenAudioDeviceStream failed: %s", SDL_GetError() );

	// Query the device buffer size for writeahead estimation.
	SDL_AudioSpec got;
	int sample_frames = 0;
	SDL_AudioDeviceID devid = SDL_GetAudioStreamDevice( m_pStream );
	if( SDL_GetAudioDeviceFormat(devid, &got, &sample_frames) )
	{
		m_iWriteahead = (sample_frames > 0) ? sample_frames : 1024;
	}
	else
	{
		m_iWriteahead = 1024;
	}

	LOG->Trace( "SDL audio: %d Hz, buffer %d frames (%.1f ms)",
	            m_iSampleRate, m_iWriteahead,
	            m_iWriteahead * 1000.0f / m_iSampleRate );

	SetDecodeBufferSize( m_iWriteahead * 2 );
	StartDecodeThread();

	SDL_ResumeAudioStreamDevice( m_pStream );

	LOG->Trace( "SDL sound driver started successfully" );
	return std::string();
}

void RageSoundDriver_SDL::AudioCallback( void *userdata, SDL_AudioStream *stream,
                                          int additional_amount, int total_amount )
{
	RageSoundDriver_SDL *pThis = static_cast<RageSoundDriver_SDL *>( userdata );

	if( additional_amount <= 0 )
		return;

	const int iBytesPerFrame = channels * sizeof(int16_t);
	int iFrames = additional_amount / iBytesPerFrame;
	if( iFrames <= 0 )
		return;

	int16_t *pBuf = new int16_t[iFrames * channels];
	std::memset( pBuf, 0, iFrames * channels * sizeof(int16_t) );

	int64_t iCurrentFrame = pThis->m_iFramesWritten;
	pThis->Mix( pBuf, iFrames, iCurrentFrame, iCurrentFrame );
	pThis->m_iFramesWritten += iFrames;

	SDL_PutAudioStreamData( stream, pBuf, iFrames * iBytesPerFrame );
	delete[] pBuf;
}

int64_t RageSoundDriver_SDL::GetPosition() const
{
	// m_iFramesWritten tracks how many frames have been mixed into the SDL
	// stream. The actual playback position lags behind by the queued amount.
	// SDL_GetAudioStreamQueued tells us how many bytes are still buffered.
	int iQueued = SDL_GetAudioStreamQueued( m_pStream );
	const int iBytesPerFrame = channels * sizeof(int16_t);
	int iQueuedFrames = iQueued / iBytesPerFrame;

	int64_t pos = m_iFramesWritten - iQueuedFrames;
	return (pos > 0) ? pos : 0;
}

float RageSoundDriver_SDL::GetPlayLatency() const
{
	return static_cast<float>( m_iWriteahead ) / static_cast<float>( m_iSampleRate );
}

int RageSoundDriver_SDL::GetSampleRate() const
{
	return m_iSampleRate;
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
