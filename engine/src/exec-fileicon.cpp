/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// exec-fileicon.cpp
//
// Platform-agnostic exec layer for iconDataForFile() and
// iconDataForExtension().  Each function:
//   1. Delegates to the platform-specific MCFileIconGet* function to obtain
//      an MCImageBitmap at the requested size.
//   2. Encodes that bitmap as PNG using the engine's existing codec.
//   3. Returns the raw PNG bytes as an MCDataRef.
//
// On failure the result is set to an error string and r_data is left
// unmodified (the caller receives empty).

#include "prefix.h"

#include "exec.h"
#include "exec-fileicon.h"
#include "image.h"
#include "imagebitmap.h"
#include "mcio.h"

////////////////////////////////////////////////////////////////////////////////

static const uinteger_t kMCFileIconDefaultSize = 32;

// Shared helper: encode an MCImageBitmap as PNG into an MCDataRef.
static bool s_encode_bitmap_as_png(MCImageBitmap *p_bitmap, MCDataRef &r_data)
{
    IO_handle t_stream = MCS_fakeopenwrite();
    if (t_stream == nil)
        return false;

    uindex_t t_bytes_written = 0;
    bool t_ok = MCImageEncodePNG(p_bitmap, nil, t_stream, t_bytes_written);

    void *t_buffer = nil;
    uint32_t t_size = 0;
    if (MCS_closetakingbuffer_uint32(t_stream, t_buffer, t_size) != IO_NORMAL)
        t_ok = false;

    if (t_ok)
        t_ok = MCDataCreateWithBytes((const byte_t *)t_buffer, t_size, r_data);

    MCMemoryDeallocate(t_buffer);
    return t_ok;
}

////////////////////////////////////////////////////////////////////////////////

void MCFilesEvalIconDataForFile(MCExecContext &ctxt,
                                MCStringRef p_path,
                                uinteger_t p_size,
                                MCDataRef &r_data)
{
    MCImageBitmap *t_bitmap = nil;
    if (!MCFileIconGetForFile(p_path, p_size, t_bitmap))
    {
        ctxt.SetTheResultToCString("iconDataForFile: could not retrieve icon");
        return;
    }

    MCDataRef t_data;
    if (!s_encode_bitmap_as_png(t_bitmap, t_data))
    {
        MCImageFreeBitmap(t_bitmap);
        ctxt.SetTheResultToCString("iconDataForFile: could not encode icon as PNG");
        return;
    }

    MCImageFreeBitmap(t_bitmap);
    r_data = t_data;
}

void MCFilesEvalIconDataForExtension(MCExecContext &ctxt,
                                     MCStringRef p_extension,
                                     uinteger_t p_size,
                                     MCDataRef &r_data)
{
    MCImageBitmap *t_bitmap = nil;
    if (!MCFileIconGetForExtension(p_extension, p_size, t_bitmap))
    {
        ctxt.SetTheResultToCString("iconDataForExtension: could not retrieve icon");
        return;
    }

    MCDataRef t_data;
    if (!s_encode_bitmap_as_png(t_bitmap, t_data))
    {
        MCImageFreeBitmap(t_bitmap);
        ctxt.SetTheResultToCString("iconDataForExtension: could not encode icon as PNG");
        return;
    }

    MCImageFreeBitmap(t_bitmap);
    r_data = t_data;
}
