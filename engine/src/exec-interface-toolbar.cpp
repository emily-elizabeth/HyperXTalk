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

#include "exec.h"
#include "exec-interface.h"
#include "toolbar.h"
#include "image.h"

////////////////////////////////////////////////////////////////////////////////
// Display mode enum type

static MCExecEnumTypeElementInfo _kMCInterfaceToolbarDisplayModeElementInfo[] =
{
    { "default",      kMCToolbarDisplayModeDefault,      false },
    { "iconAndLabel", kMCToolbarDisplayModeIconAndLabel, false },
    { "iconOnly",     kMCToolbarDisplayModeIconOnly,     false },
    { "labelOnly",    kMCToolbarDisplayModeLabelOnly,    false },
};

static MCExecEnumTypeInfo _kMCInterfaceToolbarDisplayModeTypeInfo =
{
    "Interface.ToolbarDisplayMode",
    sizeof(_kMCInterfaceToolbarDisplayModeElementInfo) /
        sizeof(MCExecEnumTypeElementInfo),
    _kMCInterfaceToolbarDisplayModeElementInfo,
};

MCExecEnumTypeInfo *kMCInterfaceToolbarDisplayModeTypeInfo =
    &_kMCInterfaceToolbarDisplayModeTypeInfo;

////////////////////////////////////////////////////////////////////////////////
// Toolbar property accessors

void MCToolbar::GetDisplayMode(MCExecContext& ctxt, intenum_t& r_mode)
{
    r_mode = (intenum_t)m_display_mode;
}

void MCToolbar::SetDisplayMode(MCExecContext& ctxt, intenum_t p_mode)
{
    m_display_mode = (MCToolbarDisplayMode)p_mode;
    if (m_backend != nil)
        m_backend->SetDisplayMode(m_display_mode);
}

void MCToolbar::GetToolbarVisible(MCExecContext& ctxt, bool& r_visible)
{
    r_visible = m_toolbar_visible;
}

void MCToolbar::SetToolbarVisible(MCExecContext& ctxt, bool p_visible)
{
    m_toolbar_visible = p_visible;
    if (m_backend != nil)
        m_backend->SetVisible(p_visible);
}

void MCToolbar::GetItemNames(MCExecContext& ctxt, MCStringRef& r_names)
{
    MCAutoListRef t_list;
    /* UNCHECKED */ MCListCreateMutable('\n', &t_list);
    for (uindex_t i = 0; i < m_item_count; i++)
    {
        /* UNCHECKED */ MCListAppend(*t_list,
                                     MCNameGetString(m_items[i].GetName()));
    }
    /* UNCHECKED */ MCListCopyAsString(*t_list, r_names);
}

void MCToolbar::SetItemNames(MCExecContext& ctxt, MCStringRef p_names)
{
    // Parse the newline-delimited list.  Items present in the current list
    // but absent from p_names are removed; surviving items are reordered to
    // match the requested order.  Names not currently in the toolbar are
    // ignored (use SetItemLabel to create new items).

    // 1. Split p_names into a flat array of name strings.
    uindex_t t_desired_count = 0;
    MCNameRef *t_desired = nil;

    uindex_t t_len = MCStringGetLength(p_names);
    uindex_t t_start = 0;
    for (uindex_t i = 0; i <= t_len; i++)
    {
        if (i == t_len || MCStringGetCharAtIndex(p_names, i) == '\n')
        {
            // Determine the end of the token, stripping a trailing \r so
            // that Windows-style CRLF line endings are handled correctly.
            uindex_t t_end = i;
            if (t_end > t_start &&
                MCStringGetCharAtIndex(p_names, t_end - 1) == '\r')
                t_end--;

            if (t_end > t_start)
            {
                MCAutoStringRef t_part;
                /* UNCHECKED */ MCStringCopySubstring(p_names,
                    MCRangeMake(t_start, t_end - t_start), &t_part);
                MCNameRef t_name;
                if (MCNameCreate(*t_part, t_name))
                {
                    /* UNCHECKED */ MCMemoryResizeArray(t_desired_count + 1,
                                                       t_desired,
                                                       t_desired_count);
                    t_desired[t_desired_count - 1] = t_name;
                }
            }
            t_start = i + 1;
        }
    }

    // 2. Build reordered copy of surviving items.
    //    MCMemoryResizeArray allocates raw memory (no constructors), so we use
    //    placement-new with the copy constructor to properly retain MCValueRef
    //    strings.  Using a copy-assignment via a temporary would create and
    //    immediately destroy a retaining temporary, cancelling the retain and
    //    leaving the slot with the original's (soon-to-be-freed) reference.
    MCToolbarItem *t_new_items = nil;
    uindex_t t_new_count = 0;
    for (uindex_t j = 0; j < t_desired_count; j++)
    {
        MCToolbarItem *t_item = FindItem(t_desired[j]);
        if (t_item != nil)
        {
            /* UNCHECKED */ MCMemoryResizeArray(t_new_count + 1,
                                               t_new_items, t_new_count);
            new (&t_new_items[t_new_count - 1]) MCToolbarItem(*t_item);
        }
    }

    // 3. Replace item array (old items released by _destroyItems).
    //    The new items were constructed via placement-new (copy ctor), so their
    //    MCValueRef members have independent retain counts that survive this call.
    _destroyItems();
    m_items      = t_new_items;
    m_item_count = t_new_count;

    // 4. Release the name refs we allocated.
    for (uindex_t j = 0; j < t_desired_count; j++)
        MCValueRelease(t_desired[j]);
    MCMemoryDeleteArray(t_desired);

    // 5. Sync the platform backend.
    if (m_backend != nil)
        _syncBackendItems();
}

////////////////////////////////////////////////////////////////////////////////
// Per-item property accessors

void MCToolbar::GetItemLabel(MCExecContext& ctxt, MCNameRef p_item,
                             MCStringRef& r_label)
{
    MCToolbarItem *t_item = FindItem(p_item);
    if (t_item == nil)
    {
        r_label = MCValueRetain(kMCEmptyString);
        return;
    }
    r_label = MCValueRetain(t_item->GetLabel() ? t_item->GetLabel()
                                                : kMCEmptyString);
}

void MCToolbar::SetItemLabel(MCExecContext& ctxt, MCNameRef p_item,
                             MCStringRef p_label)
{
    fprintf(stderr, "[SetItemLabel] called on toolbar %p item=%p\n",
            (void*)this, (void*)p_item);
    fflush(stderr);
    MCToolbarItem *t_item = FindItem(p_item);
    if (t_item == nil)
    {
        // Auto-create: setting the label for a new item name creates that item.
        AddItem(p_item, p_label, kMCEmptyString, kMCEmptyString,
                kMCToolbarItemStyleButton);
        return;
    }
    t_item->SetLabel(p_label);
    if (m_backend != nil)
        m_backend->UpdateItem(t_item);
}

void MCToolbar::GetItemTooltip(MCExecContext& ctxt, MCNameRef p_item,
                               MCStringRef& r_tooltip)
{
    MCToolbarItem *t_item = FindItem(p_item);
    if (t_item == nil)
    {
        r_tooltip = MCValueRetain(kMCEmptyString);
        return;
    }
    r_tooltip = MCValueRetain(t_item->GetTooltip() ? t_item->GetTooltip()
                                                   : kMCEmptyString);
}

void MCToolbar::SetItemTooltip(MCExecContext& ctxt, MCNameRef p_item,
                               MCStringRef p_tooltip)
{
    MCToolbarItem *t_item = FindItem(p_item);
    if (t_item == nil)
        return;
    t_item->SetTooltip(p_tooltip);
    if (m_backend != nil)
        m_backend->UpdateItem(t_item);
}

void MCToolbar::GetItemEnabled(MCExecContext& ctxt, MCNameRef p_item,
                               bool& r_enabled)
{
    MCToolbarItem *t_item = FindItem(p_item);
    r_enabled = (t_item != nil) ? t_item->GetEnabled() : false;
}

void MCToolbar::SetItemEnabled(MCExecContext& ctxt, MCNameRef p_item,
                               bool p_enabled)
{
    MCToolbarItem *t_item = FindItem(p_item);
    if (t_item == nil)
        return;
    t_item->SetEnabled(p_enabled);
    if (m_backend != nil)
        m_backend->UpdateItem(t_item);
}

void MCToolbar::GetItemIcon(MCExecContext& ctxt, MCNameRef p_item,
                            MCStringRef& r_icon)
{
    MCToolbarItem *t_item = FindItem(p_item);
    if (t_item == nil)
    {
        r_icon = MCValueRetain(kMCEmptyString);
        return;
    }
    r_icon = MCValueRetain(t_item->GetIcon() ? t_item->GetIcon()
                                             : kMCEmptyString);
}

void MCToolbar::SetItemIcon(MCExecContext& ctxt, MCNameRef p_item,
                            MCStringRef p_icon)
{
    MCToolbarItem *t_item = FindItem(p_item);
    if (t_item == nil)
        return;

    // SetIcon clears any previously cached image data.
    t_item->SetIcon(p_icon);

    // Try to resolve the name as an engine MCImage object and cache its PNG
    // bytes.  This is tried even when the name might also match an SF Symbol
    // or bundle image: a stack image with the same name wins, which is the
    // most natural behaviour (the developer named it that way deliberately).
    if (!MCStringIsEmpty(p_icon))
    {
        MCImage *t_image = resolveimagename(p_icon);
        if (t_image != nil)
        {
            MCAutoDataRef t_data;
            t_image->GetText(ctxt, &t_data);
            if (*t_data != nil && !MCDataIsEmpty(*t_data))
                t_item->SetImageData(*t_data);
        }
    }

    if (m_backend != nil)
        m_backend->UpdateItem(t_item);
}

void MCToolbar::GetItemStyle(MCExecContext& ctxt, MCNameRef p_item,
                             MCStringRef& r_style)
{
    MCToolbarItem *t_item = FindItem(p_item);
    if (t_item == nil)
    {
        r_style = MCValueRetain(kMCEmptyString);
        return;
    }
    const char *t_str;
    switch (t_item->GetStyle())
    {
        case kMCToolbarItemStyleSeparator: t_str = "separator"; break;
        case kMCToolbarItemStyleSpace:     t_str = "space";     break;
        case kMCToolbarItemStyleFlexSpace: t_str = "flexSpace"; break;
        default:                           t_str = "button";    break;
    }
    /* UNCHECKED */ MCStringCreateWithCString(t_str, r_style);
}

void MCToolbar::SetItemStyle(MCExecContext& ctxt, MCNameRef p_item,
                             MCStringRef p_style)
{
    MCToolbarItem *t_item = FindItem(p_item);
    if (t_item == nil)
        return;

    MCToolbarItemStyle t_new_style = kMCToolbarItemStyleButton;
    if (MCStringIsEqualToCString(p_style, "separator", kMCCompareCaseless))
        t_new_style = kMCToolbarItemStyleSeparator;
    else if (MCStringIsEqualToCString(p_style, "space", kMCCompareCaseless))
        t_new_style = kMCToolbarItemStyleSpace;
    else if (MCStringIsEqualToCString(p_style, "flexSpace", kMCCompareCaseless))
        t_new_style = kMCToolbarItemStyleFlexSpace;

    t_item->SetStyle(t_new_style);
    if (m_backend != nil)
        m_backend->UpdateItem(t_item);
}
