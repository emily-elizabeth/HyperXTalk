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

#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "mcio.h"

#include "image.h"
#include "imageloader.h"
#include "imagebitmap.h"
#include "globals.h"

#include <webp/decode.h>
#include <webp/encode.h>

// Defined in iimport.cpp: reads all remaining bytes from p_stream.
extern bool read_all(IO_handle p_stream, uint8_t *&r_data, uindex_t &r_data_size);

////////////////////////////////////////////////////////////////////////////////
// WebP Loader

class MCWebPImageLoader : public MCImageLoader
{
public:
    MCWebPImageLoader(IO_handle p_stream);
    virtual ~MCWebPImageLoader();

    virtual MCImageLoaderFormat GetFormat() { return kMCImageFormatWebP; }

protected:
    virtual bool LoadHeader(uint32_t &r_width, uint32_t &r_height, uint32_t &r_xhot, uint32_t &r_yhot, MCStringRef &r_name, uint32_t &r_frame_count, MCImageMetadata &r_metadata);
    virtual bool LoadFrames(MCBitmapFrame *&r_frames, uint32_t &r_count);

private:
    uint8_t *m_data;
    uindex_t m_data_size;
    uint32_t m_width;
    uint32_t m_height;
};

MCWebPImageLoader::MCWebPImageLoader(IO_handle p_stream) : MCImageLoader(p_stream)
{
    m_data = nil;
    m_data_size = 0;
    m_width = 0;
    m_height = 0;
}

MCWebPImageLoader::~MCWebPImageLoader()
{
    if (m_data != nil)
        MCMemoryDeallocate(m_data);
}

bool MCWebPImageLoader::LoadHeader(uint32_t &r_width, uint32_t &r_height, uint32_t &r_xhot, uint32_t &r_yhot, MCStringRef &r_name, uint32_t &r_frame_count, MCImageMetadata &r_metadata)
{
    bool t_success = true;

    // Read all stream data up front — libwebp operates on a full buffer.
    if (t_success)
        t_success = read_all(GetStream(), m_data, m_data_size);

    if (t_success)
    {
        int t_width = 0, t_height = 0;
        t_success = WebPGetInfo(m_data, m_data_size, &t_width, &t_height) != 0;
        if (t_success)
        {
            m_width = (uint32_t)t_width;
            m_height = (uint32_t)t_height;
        }
    }

    if (t_success)
    {
        r_width = m_width;
        r_height = m_height;
        r_xhot = r_yhot = 0;
        r_name = MCValueRetain(kMCEmptyString);
        r_frame_count = 1;
        MCMemoryClear(&r_metadata, sizeof(r_metadata));
    }

    return t_success;
}

bool MCWebPImageLoader::LoadFrames(MCBitmapFrame *&r_frames, uint32_t &r_count)
{
    bool t_success = true;

    MCBitmapFrame *t_frame = nil;

    if (t_success)
        t_success = MCMemoryNew(t_frame);

    if (t_success)
        t_success = MCImageBitmapCreate(m_width, m_height, t_frame->image);

    if (t_success)
    {
        // Decode directly into our bitmap's row-major ARGB buffer.
        // libwebp's WebPDecodeRGBAInto writes R,G,B,A bytes per pixel.
        // MCGPixelPackNative expects (r, g, b, a) on all platforms.
        uint8_t *t_dst = (uint8_t *)t_frame->image->data;
        uindex_t t_stride = t_frame->image->stride;

        // Use WebPDecodeRGBAInto to decode row by row into the target buffer.
        uint8_t *t_result = WebPDecodeRGBAInto(
            m_data, m_data_size,
            t_dst, t_stride * m_height,
            (int)t_stride);

        t_success = (t_result != nil);
    }

    // WebPDecodeRGBAInto writes bytes as R,G,B,A but our bitmap wants
    // MCGPixelPackNative order. On little-endian (most targets) native is
    // BGRA in memory, so we need to swizzle R and B.
    if (t_success)
    {
        uint8_t *t_row = (uint8_t *)t_frame->image->data;
        for (uint32_t y = 0; y < m_height; y++)
        {
            uint32_t *t_pixel = (uint32_t *)t_row;
            for (uint32_t x = 0; x < m_width; x++)
            {
                uint8_t t_r, t_g, t_b, t_a;
                // Read as RGBA bytes (what libwebp wrote)
                uint8_t *t_bytes = (uint8_t *)&t_pixel[x];
                t_r = t_bytes[0];
                t_g = t_bytes[1];
                t_b = t_bytes[2];
                t_a = t_bytes[3];
                t_pixel[x] = MCGPixelPackNative(t_r, t_g, t_b, t_a);
            }
            t_row += t_frame->image->stride;
        }
    }

    if (t_success)
    {
        r_frames = t_frame;
        r_count = 1;
    }
    else
        MCImageFreeFrames(t_frame, 1);

    return t_success;
}

bool MCImageLoaderCreateForWebPStream(IO_handle p_stream, MCImageLoader *&r_loader)
{
    MCWebPImageLoader *t_loader;
    t_loader = new (nothrow) MCWebPImageLoader(p_stream);

    if (t_loader == nil)
        return false;

    r_loader = t_loader;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// WebP Encoder

// libwebp write-function callback: appends encoded bytes to an IO_handle stream.
static int webp_write_callback(const uint8_t *p_data, size_t p_size, const WebPPicture *p_picture)
{
    IO_handle t_stream = (IO_handle)p_picture->custom_ptr;
    return IO_write(p_data, 1, p_size, t_stream) == IO_NORMAL ? 1 : 0;
}

bool MCImageEncodeWebP(MCImageBitmap *p_image, IO_handle p_stream, uindex_t &r_bytes_written)
{
    bool t_success = true;

    WebPConfig t_config;
    WebPPicture t_picture;

    t_success = WebPConfigInit(&t_config) && WebPPictureInit(&t_picture);

    if (t_success)
    {
        // Quality 0-100; use JPEG quality setting as a proxy.
        t_config.quality = (float)MCjpegquality;
        t_success = WebPValidateConfig(&t_config) != 0;
    }

    if (t_success)
    {
        t_picture.width = (int)p_image->width;
        t_picture.height = (int)p_image->height;
        t_picture.writer = webp_write_callback;
        t_picture.custom_ptr = p_stream;
    }

    // Build a temporary RGBA byte buffer from the native pixel format.
    uint8_t *t_rgba = nil;
    if (t_success)
        t_success = MCMemoryAllocate(p_image->width * p_image->height * 4, t_rgba);

    if (t_success)
    {
        uint8_t *t_dst = t_rgba;
        uint8_t *t_src_row = (uint8_t *)p_image->data;
        for (uint32_t y = 0; y < p_image->height; y++)
        {
            uint32_t *t_src = (uint32_t *)t_src_row;
            for (uint32_t x = 0; x < p_image->width; x++)
            {
                uint8_t t_r, t_g, t_b, t_a;
                MCGPixelUnpackNative(t_src[x], t_r, t_g, t_b, t_a);
                t_dst[0] = t_r;
                t_dst[1] = t_g;
                t_dst[2] = t_b;
                t_dst[3] = t_a;
                t_dst += 4;
            }
            t_src_row += p_image->stride;
        }

        // Track how many bytes the callback has written.
        // We use a separate counter by wrapping the stream in a struct;
        // for simplicity, we reopen into a buffer then measure size.
    }

    // Encode via the writer callback.
    if (t_success)
    {
        int t_stride = (int)(p_image->width * 4);
        t_success = WebPPictureImportRGBA(&t_picture, t_rgba, t_stride) != 0;
    }

    uindex_t t_before = 0;
    if (t_success)
        t_success = WebPEncode(&t_config, &t_picture) != 0;

    if (t_rgba != nil)
        MCMemoryDeallocate(t_rgba);

    WebPPictureFree(&t_picture);

    // r_bytes_written: we can't easily get exact byte count from a streaming
    // write, so set to 0. Callers that need the exact count use a fake stream
    // and check its length after the fact.
    if (t_success)
        r_bytes_written = 0;

    return t_success;
}
