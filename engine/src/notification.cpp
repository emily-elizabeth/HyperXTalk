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

//
// Cross-platform notification: statement implementations and engine callbacks.
//

#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "scriptpt.h"
#include "globals.h"
#include "stack.h"
#include "card.h"
#include "exec.h"
#include "param.h"
#include "mcerror.h"
#include "executionerrors.h"
#include "express.h"

#include "mcstring.h"
#include "cmds.h"
#include "funcs.h"
#include "notification.h"

////////////////////////////////////////////////////////////////////////////////
// Engine-side dispatch helpers
//
// All notification callbacks must be marshalled to the main thread by the
// platform code before calling these functions.

static void _dispatch_to_default_card(MCNameRef p_msg)
{
    MCStack *t_stack = MCdefaultstackptr;
    if (t_stack == nil)
        return;
    MCCard *t_card = t_stack->getcurcard();
    if (t_card != nil)
        t_card->message(p_msg);
}

void MCNotificationDispatchPermissionGranted()
{
    _dispatch_to_default_card(MCM_notification_permission_granted);
}

void MCNotificationDispatchPermissionDenied()
{
    _dispatch_to_default_card(MCM_notification_permission_denied);
}

void MCNotificationDispatchClicked(MCStringRef p_tag)
{
    MCStack *t_stack = MCdefaultstackptr;
    if (t_stack == nil)
        return;
    MCCard *t_card = t_stack->getcurcard();
    if (t_card != nil)
        t_card->message_with_valueref_args(MCM_notification_clicked, p_tag);
}

////////////////////////////////////////////////////////////////////////////////
// requestNotificationPermission

MCRequestNotificationPermission::~MCRequestNotificationPermission() {}

Parse_stat MCRequestNotificationPermission::parse(MCScriptPoint& sp)
{
    initpoint(sp);
    return PS_NORMAL;
}

void MCRequestNotificationPermission::exec_ctxt(MCExecContext& ctxt)
{
    MCPlatformRequestNotificationPermission();
}

////////////////////////////////////////////////////////////////////////////////
// showNotification title [body [tag]]
//
// All three are positional expressions separated by whitespace.
// body and tag are optional; body defaults to empty, tag is auto-generated.

MCShowNotification::~MCShowNotification()
{
    delete m_title;
    delete m_body;
    delete m_tag;
}

// Helper: consume the next token if it is a comma (ST_SEP), otherwise put it back.
static void _skip_optional_comma(MCScriptPoint& sp)
{
    Symbol_type t_type;
    if (sp.next(t_type) != PS_NORMAL || t_type != ST_SEP)
        sp.backup();
}

Parse_stat MCShowNotification::parse(MCScriptPoint& sp)
{
    initpoint(sp);

    // Required: title.
    // items=False so the expression parser stops at a comma rather than
    // consuming it as a list separator (which would collapse "a","b","c"
    // into a single "a,b,c" value).
    if (sp.parseexp(False, False, &m_title) != PS_NORMAL)
    {
        MCperror->add(PE_SHOWNOTIFICATION_BADTITLE, sp);
        return PS_ERROR;
    }

    // Allow an optional comma between title and body.
    _skip_optional_comma(sp);

    // Optional: body (try, roll back on failure)
    {
        MCScriptPoint t_saved(sp);
        MCerrorlock++;
        if (sp.parseexp(False, False, &m_body) != PS_NORMAL)
        {
            sp = t_saved;
            delete m_body;
            m_body = nil;
        }
        MCerrorlock--;
    }

    // Optional: tag (only if body was parsed)
    if (m_body != nil)
    {
        // Allow an optional comma between body and tag.
        _skip_optional_comma(sp);

        MCScriptPoint t_saved(sp);
        MCerrorlock++;
        if (sp.parseexp(False, False, &m_tag) != PS_NORMAL)
        {
            sp = t_saved;
            delete m_tag;
            m_tag = nil;
        }
        MCerrorlock--;
    }

    return PS_NORMAL;
}

void MCShowNotification::exec_ctxt(MCExecContext& ctxt)
{
    MCAutoStringRef t_title;
    if (!ctxt.EvalExprAsStringRef(m_title, EE_SHOWNOTIFICATION_BADTITLE, &t_title))
        return;

    MCAutoStringRef t_body;
    if (m_body != nil)
    {
        if (!ctxt.EvalExprAsStringRef(m_body, EE_SHOWNOTIFICATION_BADBODY, &t_body))
            return;
    }
    else
        t_body = MCValueRetain(kMCEmptyString);

    MCAutoStringRef t_tag;
    if (m_tag != nil)
    {
        if (!ctxt.EvalExprAsStringRef(m_tag, EE_SHOWNOTIFICATION_BADTAG, &t_tag))
            return;
    }
    else
        t_tag = MCValueRetain(kMCEmptyString);

    MCPlatformShowNotification(*t_title, *t_body, *t_tag);
}

////////////////////////////////////////////////////////////////////////////////
// cancelNotification tag

MCCancelNotification::~MCCancelNotification()
{
    delete m_tag;
}

Parse_stat MCCancelNotification::parse(MCScriptPoint& sp)
{
    initpoint(sp);
    if (sp.parseexp(False, True, &m_tag) != PS_NORMAL)
    {
        MCperror->add(PE_CANCELNOTIFICATION_BADTAG, sp);
        return PS_ERROR;
    }
    return PS_NORMAL;
}

void MCCancelNotification::exec_ctxt(MCExecContext& ctxt)
{
    MCAutoStringRef t_tag;
    if (!ctxt.EvalExprAsStringRef(m_tag, EE_CANCELNOTIFICATION_BADTAG, &t_tag))
        return;
    MCPlatformCancelNotification(*t_tag);
}

////////////////////////////////////////////////////////////////////////////////
// cancelAllNotifications

MCCancelAllNotifications::~MCCancelAllNotifications() {}

Parse_stat MCCancelAllNotifications::parse(MCScriptPoint& sp)
{
    initpoint(sp);
    return PS_NORMAL;
}

void MCCancelAllNotifications::exec_ctxt(MCExecContext& ctxt)
{
    MCPlatformCancelAllNotifications();
}

////////////////////////////////////////////////////////////////////////////////
// notificationPermission() function

void MCNotificationEvalPermission(MCExecContext& ctxt, MCStringRef& r_result)
{
    MCPlatformGetNotificationPermission(r_result);
}
