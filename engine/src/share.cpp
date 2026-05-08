/* Copyright (C) 2024 HyperXTalk Contributors

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.  */

// Implementation of the `share` statement:
//
//   share <text-expr>            [from <rect-expr>]
//   share file <path-expr>       [from <rect-expr>]
//   share image <image-ref>      [from <rect-expr>]
//
// On macOS this triggers the native NSSharingServicePicker.
// On other platforms it is a documented no-op.

#include "prefix.h"

#include "globdefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "filedefs.h"

#include "scriptpt.h"
#include "handler.h"
#include "cmds.h"
#include "mcerror.h"
#include "globals.h"
#include "object.h"
#include "dispatch.h"
#include "stack.h"
#include "card.h"
#include "image.h"
#include "exec.h"
#include "util.h"
#include "sysdefs.h"
#include "platform.h"

#include "share.h"

////////////////////////////////////////////////////////////////////////////////

MCShare::MCShare()
{
    m_share_type   = kMCShareText;
    m_data         = nil;
    m_from         = nil;
    m_from_toolbar = false;
}

MCShare::~MCShare()
{
    delete m_data;
    delete m_from;
}

// Grammar:
//   share [file | image] <expr> [from <rect-expr>]
Parse_stat MCShare::parse(MCScriptPoint &sp)
{
    // Peek at the next token to check for a type modifier.
    Symbol_type t_type;
    const LT *t_lt;

    if (sp.next(t_type) == PS_NORMAL)
    {
        // Check SP_FACTOR first (for "image" as CT_IMAGE chunk type).
        if (sp.lookup(SP_FACTOR, t_lt) == PS_NORMAL &&
            t_lt->type == TT_CHUNK && t_lt->which == CT_IMAGE)
        {
            m_share_type = kMCShareImage;
            // token consumed — fall through to parse the expression
        }
        // Then check SP_ASK for "file" (AT_FILE).
        else if (sp.lookup(SP_ASK, t_lt) == PS_NORMAL &&
                 t_lt->type == TT_UNDEFINED && t_lt->which == AT_FILE)
        {
            m_share_type = kMCShareFile;
        }
        else
        {
            // Not a type modifier — put the token back.
            sp.backup();
        }
    }

    // Parse the main data expression.
    if (sp.parseexp(False, True, &m_data) != PS_NORMAL)
    {
        MCperror->add(PE_SHARE_BADDATA, sp);
        return PS_ERROR;
    }

    // Optional FROM clause: either
    //   from <rect-expr>
    //   from toolbar item <name-expr>
    if (sp.skip_token(SP_FACTOR, TT_FROM) == PS_NORMAL)
    {
        if (sp.skip_token(SP_FACTOR, TT_CHUNK, CT_TOOLBAR) == PS_NORMAL)
        {
            // Consume optional "item" keyword — tolerate "from toolbar "name""
            // as well as "from toolbar item "name"".
            sp.skip_token(SP_FACTOR, TT_CHUNK, CT_ITEM);
            m_from_toolbar = true;
        }

        if (sp.parseexp(False, True, &m_from) != PS_NORMAL)
        {
            MCperror->add(PE_SHARE_BADRECT, sp);
            return PS_ERROR;
        }
    }

    return PS_NORMAL;
}

void MCShare::exec_ctxt(MCExecContext &ctxt)
{
    // Evaluate the data expression.
    MCAutoValueRef t_value;
    if (!ctxt.EvalExprAsValueRef(m_data, EE_SHARE_BADDATA, &t_value))
        return;

    // Evaluate the optional FROM clause.
    bool             t_has_rect     = false;
    MCRectangle      t_anchor       = {0, 0, 0, 0};
    MCAutoStringRef  t_toolbar_item;           // non-nil when "from toolbar item" form
    if (m_from != nil)
    {
        MCAutoStringRef t_from_str;
        if (ctxt.EvalExprAsStringRef(m_from, EE_SHARE_BADRECT, &t_from_str))
        {
            if (m_from_toolbar)
            {
                // Store toolbar item name for platform layer.
                t_toolbar_item = MCValueRetain(*t_from_str);
            }
            else
            {
                // Parse "x1,y1,x2,y2".
                int16_t x1, y1, x2, y2;
                if (MCU_stoi2x4(*t_from_str, x1, y1, x2, y2))
                {
                    t_anchor.x      = x1;
                    t_anchor.y      = y1;
                    t_anchor.width  = (uint16_t)(x2 - x1);
                    t_anchor.height = (uint16_t)(y2 - y1);
                    t_has_rect = true;
                }
            }
        }
    }

    // Get the platform window for anchoring the popover.
    // Only Mac uses MCPlatformWindowRef meaningfully; on Windows and Linux
    // getwindow() returns a platform-native type that is not compatible, and
    // MCPlatformShareContent is a stub on those platforms so nil is correct.
    MCPlatformWindowRef t_window = nil;
#ifdef TARGET_PLATFORM_MACOS_X
    if (MCdefaultstackptr)
        t_window = MCdefaultstackptr->getwindow();
    if (t_window == nil && MCtopstackptr)
        t_window = MCtopstackptr->getwindow();
#endif

    switch (m_share_type)
    {
        case kMCShareFile:
        {
            MCAutoStringRef t_path;
            if (!ctxt.ConvertToString(*t_value, &t_path))
                return;
            MCPlatformShareContent(t_window, kMCPlatformShareFile,
                                   *t_path, t_has_rect, t_anchor, *t_toolbar_item);
            break;
        }

        case kMCShareImage:
        {
            // Resolve the named image object.
            MCAutoStringRef t_name;
            if (!ctxt.ConvertToString(*t_value, &t_name))
                return;

            MCImage *t_image = nil;
            if (MCdefaultstackptr)
            {
                MCNewAutoNameRef t_obj_name;
                if (MCNameCreate(*t_name, &t_obj_name))
                    t_image = (MCImage *)MCdefaultstackptr->getobjname(CT_IMAGE, *t_obj_name);
            }
            if (t_image == nil)
            {
                ctxt.LegacyThrow(EE_SHARE_BADIMAGE);
                return;
            }

            // Export the image in its native encoded format (PNG/JPEG/GIF),
            // which is what NSSharingServicePicker expects via NSImage.
            MCDataRef t_img_data = nil;
            if (!t_image->getclipboardtext(t_img_data) || t_img_data == nil)
            {
                ctxt.LegacyThrow(EE_SHARE_BADIMAGE);
                return;
            }

            MCPlatformShareContent(t_window, kMCPlatformShareImage,
                                   t_img_data, t_has_rect, t_anchor, *t_toolbar_item);
            MCValueRelease(t_img_data);
            break;
        }

        case kMCShareText:
        default:
        {
            MCAutoStringRef t_str;
            if (!ctxt.ConvertToString(*t_value, &t_str))
                return;
            MCPlatformShareContent(t_window, kMCPlatformShareText,
                                   *t_str, t_has_rect, t_anchor, *t_toolbar_item);
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
