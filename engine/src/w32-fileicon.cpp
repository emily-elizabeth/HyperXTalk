/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// w32-fileicon.cpp
//
// Windows implementation of MCFileIconGetForFile / MCFileIconGetForExtension.
//
// For a real path  : SHGetFileInfo with the actual path.
// For an extension : SHGetFileInfo with SHGFI_USEFILEATTRIBUTES so Windows
//                    returns the generic icon for that file type without
//                    requiring the file to exist.
//
// The HICON is rendered into a 32-bit DIB section via DrawIconEx, then the
// pixel data is unpremultiplied and copied into an MCImageBitmap.

#include "prefix.h"

#include "exec-fileicon.h"
#include "imagebitmap.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commoncontrols.h> // Full IImageList COM definition + SHGetImageList
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

////////////////////////////////////////////////////////////////////////////////

// Render an HICON into a new MCImageBitmap at p_size × p_size.
// Ownership of p_icon is NOT transferred; caller must DestroyIcon if needed.
static bool s_render_hicon(HICON p_icon,
                            uinteger_t p_size,
                            MCImageBitmap *&r_bitmap)
{
    if (p_icon == NULL)
        return false;

    // Create a 32-bpp DIB section backed by a plain memory block.
    BITMAPV5HEADER t_bmi = {};
    t_bmi.bV5Size        = sizeof(t_bmi);
    t_bmi.bV5Width       = (LONG)p_size;
    t_bmi.bV5Height      = -(LONG)p_size;   // top-down
    t_bmi.bV5Planes      = 1;
    t_bmi.bV5BitCount    = 32;
    t_bmi.bV5Compression = BI_BITFIELDS;
    // Explicit ARGB masks so GDI knows the layout.
    t_bmi.bV5RedMask     = 0x00FF0000;
    t_bmi.bV5GreenMask   = 0x0000FF00;
    t_bmi.bV5BlueMask    = 0x000000FF;
    t_bmi.bV5AlphaMask   = 0xFF000000;

    void *t_bits = NULL;
    HDC t_mem_dc = CreateCompatibleDC(NULL);
    if (t_mem_dc == NULL)
        return false;

    HBITMAP t_dib = CreateDIBSection(
        t_mem_dc,
        (const BITMAPINFO *)&t_bmi,
        DIB_RGB_COLORS,
        &t_bits,
        NULL, 0);

    if (t_dib == NULL || t_bits == NULL)
    {
        DeleteDC(t_mem_dc);
        return false;
    }

    HBITMAP t_old = (HBITMAP)SelectObject(t_mem_dc, t_dib);

    // Clear to transparent black
    memset(t_bits, 0, p_size * p_size * 4);

    // DrawIconEx renders the icon using the DIB section's alpha channel.
    DrawIconEx(t_mem_dc, 0, 0, p_icon,
               (int)p_size, (int)p_size,
               0, NULL, DI_NORMAL);

    GdiFlush();

    // Allocate the engine bitmap
    MCImageBitmap *t_bitmap = nil;
    if (!MCImageBitmapCreate(p_size, p_size, t_bitmap))
    {
        SelectObject(t_mem_dc, t_old);
        DeleteObject(t_dib);
        DeleteDC(t_mem_dc);
        return false;
    }

    // Copy and unpremultiply.
    // DrawIconEx with a 32-bpp DIB delivers pre-multiplied BGRA.
    // MCImageBitmap wants non-premultiplied ARGB (0xAARRGGBB host order).
    const uint32_t *t_src = (const uint32_t *)t_bits;
    uint32_t       *t_dst = t_bitmap->data;
    uinteger_t      t_pixel_count = p_size * p_size;

    for (uinteger_t i = 0; i < t_pixel_count; i++)
    {
        // DIB section in BITMAPV5HEADER / BI_BITFIELDS with the masks above
        // stores 0xAARRGGBB in little-endian memory, same as host uint32_t.
        uint32_t p = t_src[i];
        uint8_t  a = (uint8_t)((p >> 24) & 0xFF);

        if (a == 0)
        {
            t_dst[i] = 0;
        }
        else if (a == 255)
        {
            t_dst[i] = p;
        }
        else
        {
            // Undo premultiplication
            uint8_t r = (uint8_t)(((p >> 16) & 0xFF) * 255u / a);
            uint8_t g = (uint8_t)(((p >>  8) & 0xFF) * 255u / a);
            uint8_t b = (uint8_t)(((p       ) & 0xFF) * 255u / a);
            t_dst[i] = ((uint32_t)a << 24)
                     | ((uint32_t)r << 16)
                     | ((uint32_t)g <<  8)
                     |  (uint32_t)b;
        }
    }

    t_bitmap->has_transparency = true;
    t_bitmap->has_alpha        = true;

    SelectObject(t_mem_dc, t_old);
    DeleteObject(t_dib);
    DeleteDC(t_mem_dc);

    r_bitmap = t_bitmap;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

// Pick the best SHIL_* image-list size for the requested pixel size.
// SHGetImageList supports SHIL_SMALL (16), SHIL_LARGE (32), SHIL_EXTRALARGE
// (48), and SHIL_JUMBO (256).  We use the smallest list that is >= p_size so
// that we never upscale, then let DrawIconEx scale down if needed.
static int s_best_shil(uinteger_t p_size)
{
    if (p_size <= 16) return SHIL_SMALL;       // 16
    if (p_size <= 32) return SHIL_LARGE;       // 32
    if (p_size <= 48) return SHIL_EXTRALARGE;  // 48
    return SHIL_JUMBO;                         // 256
}

// Normalize forward slashes to backslashes in-place.
// SHGetFileInfoW is a shell namespace API and requires Win32 backslash paths.
static void s_normalize_slashes(wchar_t *p_path)
{
    for (wchar_t *p = p_path; *p; p++)
        if (*p == L'/') *p = L'\\';
}

// Retrieve an HICON for a file path using SHGetFileInfo.
// The returned HICON must be destroyed with DestroyIcon by the caller.
static HICON s_icon_for_path(wchar_t *p_path, uinteger_t p_size)
{
    s_normalize_slashes(p_path);

    // Ask SHGetFileInfo for the system image-list index.
    SHFILEINFOW t_sfi = {};
    DWORD_PTR t_ret = SHGetFileInfoW(
        p_path, 0, &t_sfi, sizeof(t_sfi),
        SHGFI_SYSICONINDEX);

    if (!t_ret)
        return NULL;

    // Fetch the correctly-sized icon from the system image list.
    IImageList *t_iml = NULL;
    HRESULT hr = SHGetImageList(s_best_shil(p_size), __uuidof(IImageList),
                                (void **)&t_iml);
    if (FAILED(hr) || t_iml == NULL)
        return NULL;

    HICON t_icon = NULL;
    if (FAILED(t_iml->GetIcon(t_sfi.iIcon, ILD_TRANSPARENT, &t_icon)))
        t_icon = NULL;
    t_iml->Release();
    return t_icon;
}

// Retrieve an HICON for a file extension (no file needs to exist).
// Extensions are short strings like ".txt" — no slash normalization needed.
static HICON s_icon_for_ext(const wchar_t *p_ext_with_dot, uinteger_t p_size)
{
    SHFILEINFOW t_sfi = {};
    DWORD_PTR t_ret = SHGetFileInfoW(
        p_ext_with_dot,
        FILE_ATTRIBUTE_NORMAL,
        &t_sfi, sizeof(t_sfi),
        SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);

    if (!t_ret)
        return NULL;

    IImageList *t_iml = NULL;
    HRESULT hr = SHGetImageList(s_best_shil(p_size), __uuidof(IImageList),
                                (void **)&t_iml);
    if (FAILED(hr) || t_iml == NULL)
        return NULL;

    HICON t_icon = NULL;
    if (FAILED(t_iml->GetIcon(t_sfi.iIcon, ILD_TRANSPARENT, &t_icon)))
        t_icon = NULL;
    t_iml->Release();
    return t_icon;
}

////////////////////////////////////////////////////////////////////////////////

bool MCFileIconGetForFile(MCStringRef p_path,
                          uinteger_t p_size,
                          MCImageBitmap *&r_bitmap)
{
    // Convert MCStringRef to wide string for Shell APIs.
    uindex_t t_len = MCStringGetLength(p_path);
    MCAutoArray<unichar_t> t_buf;
    if (!t_buf.New(t_len + 1))
        return false;

    MCStringGetChars(p_path, MCRangeMake(0, t_len), t_buf.Ptr());
    t_buf[t_len] = 0;

    HICON t_icon = s_icon_for_path((wchar_t *)t_buf.Ptr(), p_size);
    if (t_icon == NULL)
        return false;

    bool t_ok = s_render_hicon(t_icon, p_size, r_bitmap);
    DestroyIcon(t_icon);
    return t_ok;
}

bool MCFileIconGetForExtension(MCStringRef p_extension,
                               uinteger_t p_size,
                               MCImageBitmap *&r_bitmap)
{
    uindex_t t_len = MCStringGetLength(p_extension);

    // We need to pass an extension with a leading dot, e.g. L".txt".
    // Allocate space for dot + extension + NUL.
    MCAutoArray<unichar_t> t_buf;
    if (!t_buf.New(t_len + 2))
        return false;

    const unichar_t *t_chars = MCStringGetCharPtr(p_extension);

    if (t_len > 0 && t_chars[0] == (unichar_t)'.')
    {
        // Already has a dot
        MCStringGetChars(p_extension, MCRangeMake(0, t_len), t_buf.Ptr());
        t_buf[t_len] = 0;
    }
    else
    {
        // Prepend dot
        t_buf[0] = (unichar_t)'.';
        MCStringGetChars(p_extension, MCRangeMake(0, t_len), t_buf.Ptr() + 1);
        t_buf[t_len + 1] = 0;
    }

    HICON t_icon = s_icon_for_ext((const wchar_t *)t_buf.Ptr(), p_size);
    if (t_icon == NULL)
        return false;

    bool t_ok = s_render_hicon(t_icon, p_size, r_bitmap);
    DestroyIcon(t_icon);
    return t_ok;
}
