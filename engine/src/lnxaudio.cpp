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
	X11Audio : see lnxaudio.h.

	The PulseAudio simple API is synchronous: pa_simple_write() blocks until
	the server has taken the data. A clip is therefore streamed from a worker
	thread in short chunks, so the engine's event loop keeps running and a
	'play stop' takes effect within a chunk or so. aclip polls isplaying()
	from its timer to notice the end of the clip and send playStopped.

	Only the handful of libpulse-simple symbols used here are declared, with
	the layouts from <pulse/sample.h>, <pulse/def.h> and <pulse/simple.h>.
	The library itself is dlopen'd through linux.stubs, so there is no
	build-time dependency on the PulseAudio headers or libraries.
*/

#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "lnxaudio.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// PulseAudio simple API. Resolved at runtime by the pulse_simple module in
// linux.stubs; the struct names differ from the public headers only so they
// cannot clash with the copies lnx-speech.cpp declares for the record side.

typedef struct pa_simple pa_simple;

struct MCPulseSampleSpec
{
	int32_t format;         // pa_sample_format_t
	uint32_t rate;
	uint8_t channels;
};

struct MCPulseBufferAttr
{
	uint32_t maxlength;
	uint32_t tlength;
	uint32_t prebuf;
	uint32_t minreq;
	uint32_t fragsize;
};

enum
{
	kMCPulseStreamPlayback = 1,     // PA_STREAM_PLAYBACK
};

enum
{
	kMCPulseSampleU8 = 0,           // PA_SAMPLE_U8
	kMCPulseSampleS16BE = 4,        // PA_SAMPLE_S16BE
	kMCPulseSampleS24BE = 10,       // PA_SAMPLE_S24BE
};

extern "C"
{
	pa_simple *pa_simple_new(const char *p_server, const char *p_name,
	                         int p_direction, const char *p_device,
	                         const char *p_stream_name,
	                         const MCPulseSampleSpec *p_spec,
	                         const void *p_channel_map,
	                         const MCPulseBufferAttr *p_attr, int *r_error);
	int pa_simple_write(pa_simple *p_stream, const void *p_data,
	                    size_t p_bytes, int *r_error);
	int pa_simple_drain(pa_simple *p_stream, int *r_error);
	int pa_simple_flush(pa_simple *p_stream, int *r_error);
	void pa_simple_free(pa_simple *p_stream);

	int initialise_weak_link_pulse_simple(void);
}

////////////////////////////////////////////////////////////////////////////////

// How much audio each pa_simple_write() hands over. stop() is noticed between
// writes, so this bounds how long a 'play stop' takes to be heard.
static const uint32_t kMCAudioChunkMilliseconds = 50;

// Target server-side buffer length. Short, for the same reason.
static const uint32_t kMCAudioBufferMilliseconds = 200;

uint2 X11Audio::s_loudness = 100;

////////////////////////////////////////////////////////////////////////////////

// Scale the samples in place by p_gain percent.
static void MCAudioApplyGain(uint1 *p_buffer, uint4 p_size, uint2 p_width,
                             int32_t p_gain)
{
	if (p_gain == 100)
		return;

	switch (p_width)
	{
	case 1:
		for (uint4 i = 0; i < p_size; i++)
		{
			int32_t t_sample = ((int32_t)p_buffer[i] - 128) * p_gain / 100;
			p_buffer[i] = (uint1)(t_sample + 128);
		}
		break;

	case 2:
		for (uint4 i = 0; i + 1 < p_size; i += 2)
		{
			int32_t t_sample =
			    (int16_t)(((uint16_t)p_buffer[i] << 8) | p_buffer[i + 1]);
			t_sample = t_sample * p_gain / 100;
			p_buffer[i] = (uint1)(t_sample >> 8);
			p_buffer[i + 1] = (uint1)t_sample;
		}
		break;

	case 3:
		for (uint4 i = 0; i + 2 < p_size; i += 3)
		{
			// Assemble in the top 24 bits so the shift sign-extends.
			int32_t t_sample =
			    (int32_t)(((uint32_t)p_buffer[i] << 24) |
			              ((uint32_t)p_buffer[i + 1] << 16) |
			              ((uint32_t)p_buffer[i + 2] << 8)) >> 8;
			t_sample = t_sample * p_gain / 100;
			p_buffer[i] = (uint1)(t_sample >> 16);
			p_buffer[i + 1] = (uint1)(t_sample >> 8);
			p_buffer[i + 2] = (uint1)t_sample;
		}
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////

X11Audio::X11Audio(void)
{
	m_stream = NULL;
	m_active = false;
	m_stop_requested = false;
	m_finished = false;
	m_buffer = NULL;
	m_size = 0;
	m_chunk_size = 0;
	m_looping = false;
}

X11Audio::~X11Audio(void)
{
	stop();
}

bool X11Audio::isactive(void)
{
	return m_active;
}

bool X11Audio::isplaying(void)
{
	return m_active && !m_finished;
}

uint2 X11Audio::getloudness(void)
{
	return s_loudness;
}

void X11Audio::setloudness(uint2 p_loudness)
{
	s_loudness = p_loudness > 100 ? 100 : p_loudness;
}

bool X11Audio::play(const int1 *p_samples, uint4 p_size, uint4 p_rate,
                    uint2 p_channels, uint2 p_width, bool p_looping,
                    uint2 p_loudness)
{
	// Reap whatever is left of the previous clip.
	stop();

	if (p_samples == NULL || p_size == 0 || p_rate == 0 || p_channels == 0)
		return false;

	int32_t t_format;
	switch (p_width)
	{
	case 1:
		t_format = kMCPulseSampleU8;
		break;
	case 2:
		t_format = kMCPulseSampleS16BE;
		break;
	case 3:
		t_format = kMCPulseSampleS24BE;
		break;
	default:
		return false;
	}

	// The generated initialiser dlopens the library on every call, so look
	// it up once.
	static int s_available = -1;
	if (s_available < 0)
		s_available = initialise_weak_link_pulse_simple() ? 1 : 0;
	if (!s_available)
		return false;

	uint32_t t_frame_size = p_width * p_channels;
	uint32_t t_bytes_per_second = t_frame_size * p_rate;

	MCPulseSampleSpec t_spec;
	t_spec.format = t_format;
	t_spec.rate = p_rate;
	t_spec.channels = (uint8_t)p_channels;

	MCPulseBufferAttr t_attr;
	t_attr.maxlength = (uint32_t)-1;
	t_attr.tlength = t_bytes_per_second * kMCAudioBufferMilliseconds / 1000;
	t_attr.prebuf = (uint32_t)-1;
	t_attr.minreq = (uint32_t)-1;
	t_attr.fragsize = (uint32_t)-1;

	int t_error = 0;
	m_stream = pa_simple_new(NULL, "HyperXTalk", kMCPulseStreamPlayback, NULL,
	                         "audioClip", &t_spec, NULL, &t_attr, &t_error);
	if (m_stream == NULL)
		return false;

	// Work on a copy: the clip may be converted or freed while the worker
	// is still writing, and the gain is applied destructively.
	m_buffer = (uint1 *)malloc(p_size);
	if (m_buffer == NULL)
	{
		release();
		return false;
	}
	memcpy(m_buffer, p_samples, p_size);

	if (p_loudness > 100)
		p_loudness = 100;
	MCAudioApplyGain(m_buffer, p_size, p_width,
	                 (int32_t)p_loudness * s_loudness / 100);

	m_size = p_size;
	m_chunk_size = t_bytes_per_second * kMCAudioChunkMilliseconds / 1000;
	m_chunk_size -= m_chunk_size % t_frame_size;
	if (m_chunk_size == 0)
		m_chunk_size = t_frame_size;
	m_looping = p_looping;
	m_stop_requested = false;
	m_finished = false;

	if (pthread_create(&m_thread, NULL, thread_main, this) != 0)
	{
		release();
		return false;
	}

	m_active = true;
	return true;
}

void X11Audio::stop(void)
{
	if (!m_active)
		return;

	m_stop_requested = true;
	pthread_join(m_thread, NULL);
	m_active = false;

	release();
}

void X11Audio::release(void)
{
	if (m_stream != NULL)
	{
		pa_simple_free(m_stream);
		m_stream = NULL;
	}
	if (m_buffer != NULL)
	{
		free(m_buffer);
		m_buffer = NULL;
	}
	m_size = 0;
}

////////////////////////////////////////////////////////////////////////////////

void *X11Audio::thread_main(void *p_context)
{
	((X11Audio *)p_context)->run();
	return NULL;
}

void X11Audio::run(void)
{
	int t_error = 0;
	bool t_ok = true;

	do
	{
		uint4 t_offset = 0;
		while (t_ok && t_offset < m_size && !m_stop_requested)
		{
			uint4 t_count = m_size - t_offset;
			if (t_count > m_chunk_size)
				t_count = m_chunk_size;

			if (pa_simple_write(m_stream, m_buffer + t_offset, t_count,
			                    &t_error) < 0)
				t_ok = false;
			else
				t_offset += t_count;
		}
	}
	while (t_ok && m_looping && !m_stop_requested);

	if (t_ok && !m_stop_requested)
		pa_simple_drain(m_stream, &t_error);    // let the tail be heard
	else
		pa_simple_flush(m_stream, &t_error);    // drop what is still queued

	m_finished = true;
}
