/* Copyright (C) 2003-2015 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

/*
	X11Audio : Provides an abstraction between the aclip object and the Linux
	audio system. It plays PCM sample buffers through PulseAudio's simple API
	(libpulse-simple), which PipeWire desktops also provide via pipewire-pulse.
	The library is loaded at runtime (see the pulse_simple module in
	linux.stubs), so the engine still runs where it is missing: audioClips are
	then silent and report playStopped straight away.
*/

#ifndef X11AUDIO_H
#define X11AUDIO_H

#include <pthread.h>
#include <atomic>

struct pa_simple;

class X11Audio
{
public:
	X11Audio(void);
	~X11Audio(void);

	// Start playing p_size bytes of interleaved PCM at p_rate Hz on a
	// background thread. Width 1 is unsigned 8-bit; widths 2 and 3 are
	// big-endian signed, which is how aclip's converters leave the samples.
	// p_loudness (0-100) scales the clip; the system playLoudness is applied
	// on top. Returns false if the sound system is unavailable.
	bool play(const int1 *p_samples, uint4 p_size, uint4 p_rate,
	          uint2 p_channels, uint2 p_width, bool p_looping,
	          uint2 p_loudness);

	// True from play() until stop(): the clip has been handed to the sound
	// system, whether or not it has finished playing.
	bool isactive(void);

	// True while samples are still being played.
	bool isplaying(void);

	// Stop playback, dropping anything still buffered, and release the stream.
	void stop(void);

	// Process-wide playback loudness (0-100), backing 'the playLoudness'.
	static uint2 getloudness(void);
	static void setloudness(uint2 p_loudness);

private:
	static void *thread_main(void *p_context);
	void run(void);
	void release(void);

	pa_simple *m_stream;
	pthread_t m_thread;
	bool m_active;
	std::atomic<bool> m_stop_requested;
	std::atomic<bool> m_finished;

	uint1 *m_buffer;
	uint4 m_size;
	uint4 m_chunk_size;
	bool m_looping;

	static uint2 s_loudness;
};

#endif
