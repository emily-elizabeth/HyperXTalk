/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// mac-fileicon.mm
//
// macOS implementation of MCFileIconGetForFile / MCFileIconGetForExtension.
//
// Uses NSWorkspace to retrieve an NSImage for a path or file type, then
// renders it into a CGBitmapContext at the requested size.  The resulting
// pixel data is unpremultiplied and copied into an MCImageBitmap.

#include "prefix.h"

#include "exec-fileicon.h"
#include "imagebitmap.h"

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

////////////////////////////////////////////////////////////////////////////////

// Render an NSImage into a new MCImageBitmap at p_size × p_size.
// Returns true and sets r_bitmap on success; caller must MCImageFreeBitmap().
static bool s_render_nsimage(NSImage *p_icon,
                              uinteger_t p_size,
                              MCImageBitmap *&r_bitmap)
{
    if (p_icon == nil)
        return false;

    CGColorSpaceRef t_colorspace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    if (t_colorspace == NULL)
        return false;

    // kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst gives us
    // pixels laid out as 0xAARRGGBB in host-byte-order (premultiplied).
    CGContextRef t_ctx = CGBitmapContextCreate(
        NULL,
        (size_t)p_size, (size_t)p_size,
        8,
        (size_t)p_size * 4,
        t_colorspace,
        (CGBitmapInfo)(kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst));

    CGColorSpaceRelease(t_colorspace);

    if (t_ctx == NULL)
        return false;

    // Clear to fully transparent
    CGContextClearRect(t_ctx, CGRectMake(0, 0, (CGFloat)p_size, (CGFloat)p_size));

    // Draw the icon at the requested size
    NSGraphicsContext *t_ns_ctx =
        [NSGraphicsContext graphicsContextWithCGContext:t_ctx flipped:NO];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:t_ns_ctx];

    [p_icon drawInRect:NSMakeRect(0, 0, (CGFloat)p_size, (CGFloat)p_size)
              fromRect:NSZeroRect
             operation:NSCompositingOperationCopy
              fraction:1.0];

    [NSGraphicsContext restoreGraphicsState];

    // Allocate the engine bitmap
    MCImageBitmap *t_bitmap = nil;
    if (!MCImageBitmapCreate(p_size, p_size, t_bitmap))
    {
        CGContextRelease(t_ctx);
        return false;
    }

    // Copy, unpremultiply, and convert pixel format.
    //
    // CGBitmapContext with kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst
    // delivers pixels as 0xAARRGGBB in host uint32_t on little-endian ARM64
    // (bytes in memory: B G R A).
    //
    // MCImageBitmap on macOS uses kMCGPixelFormatRGBA, which packs as
    // R | (G<<8) | (B<<16) | (A<<24) = 0xAABBGGRR in host uint32_t
    // (bytes in memory: R G B A).
    //
    // We extract R from bits 23-16 and B from bits 7-0 of the CG source,
    // then place them at bits 7-0 and bits 23-16 respectively in the dest.
    const uint32_t *t_src =
        (const uint32_t *)CGBitmapContextGetData(t_ctx);
    uint32_t *t_dst = t_bitmap->data;
    uinteger_t t_pixel_count = p_size * p_size;

    for (uinteger_t i = 0; i < t_pixel_count; i++)
    {
        uint32_t p = t_src[i];
        uint8_t a = (uint8_t)((p >> 24) & 0xFF);
        if (a == 0)
        {
            t_dst[i] = 0;
        }
        else if (a == 255)
        {
            // Swap R (bits 23-16) and B (bits 7-0): 0xAARRGGBB → 0xAABBGGRR.
            t_dst[i] = (p & 0xFF00FF00u)
                     | ((p >> 16) & 0xFFu)
                     | ((p & 0xFFu) << 16);
        }
        else
        {
            // Undo premultiplication, then write as 0xAABBGGRR.
            uint8_t r = (uint8_t)(((p >> 16) & 0xFF) * 255u / a);
            uint8_t g = (uint8_t)(((p >>  8) & 0xFF) * 255u / a);
            uint8_t b = (uint8_t)(((p      ) & 0xFF) * 255u / a);
            t_dst[i] = ((uint32_t)a << 24)
                     | ((uint32_t)b << 16)
                     | ((uint32_t)g <<  8)
                     |  (uint32_t)r;
        }
    }

    t_bitmap->has_transparency = true;
    t_bitmap->has_alpha        = true;

    CGContextRelease(t_ctx);
    r_bitmap = t_bitmap;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool MCFileIconGetForFile(MCStringRef p_path,
                          uinteger_t p_size,
                          MCImageBitmap *&r_bitmap)
{
    MCAutoStringRefAsCString t_cpath;
    if (!t_cpath.Lock(p_path))
        return false;

    NSString *t_ns_path =
        [NSString stringWithUTF8String:*t_cpath];
    if (t_ns_path == nil)
        return false;

    NSImage *t_icon =
        [[NSWorkspace sharedWorkspace] iconForFile:t_ns_path];

    return s_render_nsimage(t_icon, p_size, r_bitmap);
}

bool MCFileIconGetForExtension(MCStringRef p_extension,
                               uinteger_t p_size,
                               MCImageBitmap *&r_bitmap)
{
    MCAutoStringRefAsCString t_cext;
    if (!t_cext.Lock(p_extension))
        return false;

    // Strip a leading dot if the caller included one (e.g. ".txt" → "txt")
    const char *t_ext_str = *t_cext;
    if (t_ext_str[0] == '.')
        t_ext_str++;

    NSString *t_ns_ext = [NSString stringWithUTF8String:t_ext_str];
    if (t_ns_ext == nil)
        return false;

    NSImage *t_icon =
        [[NSWorkspace sharedWorkspace] iconForFileType:t_ns_ext];

    return s_render_nsimage(t_icon, p_size, r_bitmap);
}
