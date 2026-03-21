#ifndef RAGE_SOUND_DRIVER_SDL
#define RAGE_SOUND_DRIVER_SDL

#include "RageSoundDriver.h"

#include <cstdint>
#include <SDL3/SDL_audio.h>

class RageSoundDriver_SDL: public RageSoundDriver
{
public:
	RageSoundDriver_SDL();
	~RageSoundDriver_SDL();

	std::string Init();
	int64_t GetPosition() const;
	float GetPlayLatency() const;
	int GetSampleRate() const;

private:
	SDL_AudioStream *m_pStream;
	int m_iSampleRate;
	int m_iWriteahead;       // frames of buffer the device wants ahead
	int64_t m_iFramesWritten; // total frames fed to SDL

	static void AudioCallback( void *userdata, SDL_AudioStream *stream,
	                            int additional_amount, int total_amount );
};

#endif

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
