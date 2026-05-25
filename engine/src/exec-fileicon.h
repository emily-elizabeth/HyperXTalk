/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

#ifndef __MC_EXEC_FILEICON_H__
#define __MC_EXEC_FILEICON_H__

#include "imagebitmap.h"

// Platform entry points — implemented in mac-fileicon.mm, w32-fileicon.cpp,
// and lnx-fileicon.cpp respectively.
//
// Both functions return an MCImageBitmap (non-premultiplied ARGB) of
// p_size × p_size pixels, or false if no icon could be obtained.
// The caller is responsible for calling MCImageFreeBitmap on success.

bool MCFileIconGetForFile(MCStringRef p_path,
                          uinteger_t p_size,
                          MCImageBitmap *&r_bitmap);

bool MCFileIconGetForExtension(MCStringRef p_extension,
                               uinteger_t p_size,
                               MCImageBitmap *&r_bitmap);

// Exec-layer functions called by the script dispatch (funcs.cpp).
// These call the platform functions above, encode the result as PNG,
// and return it as an MCDataRef.

void MCFilesEvalIconDataForFile(MCExecContext &ctxt,
                                MCStringRef p_path,
                                uinteger_t p_size,
                                MCDataRef &r_data);

void MCFilesEvalIconDataForExtension(MCExecContext &ctxt,
                                     MCStringRef p_extension,
                                     uinteger_t p_size,
                                     MCDataRef &r_data);

#endif /* __MC_EXEC_FILEICON_H__ */
