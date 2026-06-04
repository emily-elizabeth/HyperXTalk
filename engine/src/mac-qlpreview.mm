/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// mac-qlpreview.mm
//
// After a stack is saved, write a PNG snapshot of its first card as the
// extended attribute "com.hyperxtalk.qlpreview" on the saved file.  macOS
// Quick Look reads this attribute via the bundled HyperXTalkQL.appex plugin
// and displays it as the file preview — no engine launch required.
//
// The xattr is written only when the stack has a visible on-screen window;
// if the stack is not currently open (e.g. saved programmatically without
// being displayed) this function is a silent no-op.  The existing xattr is
// left in place so older saves still show their preview.

#include "prefix.h"

#include "mac-qlpreview.h"
#include "stack.h"
#include "platform.h"

#import <Cocoa/Cocoa.h>
#import <ImageIO/ImageIO.h>

#include <sys/xattr.h>

// The name of the extended attribute that stores the PNG preview.
static const char kQLPreviewXattrName[] = "com.hyperxtalk.qlpreview";

// Maximum dimension for the stored thumbnail (pixels).  Quick Look uses up
// to 1024 × 1024 for full-screen previews; 512 gives a good balance between
// quality and xattr size.
static const CGFloat kQLPreviewMaxDimension = 512.0;

// ---------------------------------------------------------------------------

void MCStackWriteQLPreview(MCStack *p_stack, MCStringRef p_path)
{
    // ---- 1. Get the native NSWindow from the stack -------------------

    Window t_window = p_stack->getwindow();
    if (t_window == NULL)
        return;  // Stack has no window; nothing to snapshot.

    NSWindow *t_nswindow = nil;
    MCPlatformGetWindowProperty(t_window,
                                kMCPlatformWindowPropertySystemHandle,
                                kMCPlatformPropertyTypePointer,
                                &t_nswindow);
    if (t_nswindow == nil || ![t_nswindow isVisible])
        return;

    CGWindowID t_window_id = (CGWindowID)[t_nswindow windowNumber];

    // ---- 2. Capture the window contents via CoreGraphics -------------

    // kCGWindowImageBoundsIgnoreFraming crops to the content area so we
    // don't capture the title-bar shadow.
    CGImageRef t_full = CGWindowListCreateImage(
        CGRectNull,
        kCGWindowListOptionIncludingWindow,
        t_window_id,
        kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque);

    if (t_full == NULL)
        return;

    // ---- 3. Scale to at most kQLPreviewMaxDimension on the long axis -

    CGFloat t_src_w = (CGFloat)CGImageGetWidth(t_full);
    CGFloat t_src_h = (CGFloat)CGImageGetHeight(t_full);
    CGFloat t_scale = 1.0;
    if (t_src_w > kQLPreviewMaxDimension || t_src_h > kQLPreviewMaxDimension)
        t_scale = kQLPreviewMaxDimension / MAX(t_src_w, t_src_h);

    size_t t_dst_w = (size_t)(t_src_w * t_scale);
    size_t t_dst_h = (size_t)(t_src_h * t_scale);

    CGImageRef t_scaled = t_full;
    CGContextRef t_ctx = NULL;

    if (t_scale < 1.0)
    {
        // Draw into a new bitmap context at the smaller size.
        CGColorSpaceRef t_cs = CGColorSpaceCreateDeviceRGB();
        t_ctx = CGBitmapContextCreate(NULL, t_dst_w, t_dst_h, 8,
                                      t_dst_w * 4,
                                      t_cs,
                                      kCGImageAlphaPremultipliedFirst |
                                          kCGBitmapByteOrder32Host);
        CGColorSpaceRelease(t_cs);

        if (t_ctx != NULL)
        {
            CGContextSetInterpolationQuality(t_ctx, kCGInterpolationHigh);
            CGContextDrawImage(t_ctx, CGRectMake(0, 0, t_dst_w, t_dst_h),
                               t_full);
            t_scaled = CGBitmapContextCreateImage(t_ctx);
        }
        // t_scaled is now a distinct image; release the original.
        CGImageRelease(t_full);
        t_full = NULL;
    }
    // If no scaling was done, t_scaled == t_full — don't double-release.

    if (t_scaled == NULL)
    {
        if (t_ctx) CGContextRelease(t_ctx);
        return;
    }

    // ---- 4. Encode as PNG into a CFMutableData -----------------------

    CFMutableDataRef t_png_data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    if (t_png_data == NULL)
    {
        CGImageRelease(t_scaled);
        if (t_ctx) CGContextRelease(t_ctx);
        return;
    }

    // "public.png" is the UTI for PNG — use the string literal to avoid a
    // compile-time dependency on CoreServices/UniformTypeIdentifiers headers.
    CGImageDestinationRef t_dest = CGImageDestinationCreateWithData(
        t_png_data, CFSTR("public.png"), 1, NULL);

    if (t_dest != NULL)
    {
        CGImageDestinationAddImage(t_dest, t_scaled, NULL);
        CGImageDestinationFinalize(t_dest);
        CFRelease(t_dest);
    }

    // Release the image we actually encoded (may be t_full or a scaled copy).
    CGImageRelease(t_scaled);
    if (t_ctx) CGContextRelease(t_ctx);

    if (CFDataGetLength(t_png_data) == 0)
    {
        CFRelease(t_png_data);
        return;
    }

    // ---- 5. Write the PNG as an extended attribute -------------------

    MCAutoStringRefAsCString t_path_cstr;
    if (!t_path_cstr.Lock(p_path))
    {
        CFRelease(t_png_data);
        return;
    }

    setxattr(*t_path_cstr,
             kQLPreviewXattrName,
             CFDataGetBytePtr(t_png_data),
             (size_t)CFDataGetLength(t_png_data),
             0,       // position (must be 0 for non-resource-fork attrs)
             0);      // options  (0 = create or replace)

    CFRelease(t_png_data);
}
