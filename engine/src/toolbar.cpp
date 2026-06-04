#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "mcio.h"

#include "sellst.h"
#include "stack.h"
#include "card.h"
#include "mcerror.h"
#include "util.h"
#include "debug.h"
#include "globals.h"
#include "context.h"
#include "redraw.h"
#include "objectstream.h"

#include "exec.h"
#include "exec-interface.h"

#include "stackfileformat.h"
#include "toolbar.h"
#include "image.h"
#include "mcstring.h"

////////////////////////////////////////////////////////////////////////////////
// Property table

MCPropertyInfo MCToolbar::kProperties[] =
{
    DEFINE_RW_OBJ_ENUM_PROPERTY(P_TOOLBAR_DISPLAY_MODE, InterfaceToolbarDisplayMode, MCToolbar, DisplayMode)
    DEFINE_RW_OBJ_PROPERTY(P_TOOLBAR_VISIBLE, Bool, MCToolbar, ToolbarVisible)
    // itemNames is now read-write: setting it reorders/removes items.
    DEFINE_RW_OBJ_PROPERTY(P_TOOLBAR_ITEM_NAMES, String, MCToolbar, ItemNames)
    // Per-item array properties: indexed by item name.
    // Setting a label for a new name auto-creates that item.
    DEFINE_RW_OBJ_ARRAY_PROPERTY(P_TOOLBAR_ITEM_LABEL,   String, MCToolbar, ItemLabel)
    DEFINE_RW_OBJ_ARRAY_PROPERTY(P_TOOLBAR_ITEM_TOOLTIP, String, MCToolbar, ItemTooltip)
    DEFINE_RW_OBJ_ARRAY_PROPERTY(P_TOOLBAR_ITEM_ENABLED, Bool,   MCToolbar, ItemEnabled)
    DEFINE_RW_OBJ_ARRAY_PROPERTY(P_TOOLBAR_ITEM_ICON,    String, MCToolbar, ItemIcon)
    DEFINE_RW_OBJ_ARRAY_PROPERTY(P_TOOLBAR_ITEM_STYLE,   String, MCToolbar, ItemStyle)
};

MCObjectPropertyTable MCToolbar::kPropertyTable =
{
    &MCControl::kPropertyTable,
    sizeof(kProperties) / sizeof(kProperties[0]),
    &kProperties[0],
};

////////////////////////////////////////////////////////////////////////////////
// MCToolbarItem

MCToolbarItem::MCToolbarItem()
    : m_name(nil), m_label(nil), m_tooltip(nil), m_icon(nil),
      m_image_data(nil), m_enabled(true), m_style(kMCToolbarItemStyleButton)
{
}

MCToolbarItem::MCToolbarItem(const MCToolbarItem &p_ref)
    : m_name(nil), m_label(nil), m_tooltip(nil), m_icon(nil),
      m_image_data(nil), m_enabled(p_ref.m_enabled), m_style(p_ref.m_style)
{
    if (p_ref.m_name       != nil) m_name       = MCValueRetain(p_ref.m_name);
    if (p_ref.m_label      != nil) m_label      = MCValueRetain(p_ref.m_label);
    if (p_ref.m_tooltip    != nil) m_tooltip    = MCValueRetain(p_ref.m_tooltip);
    if (p_ref.m_icon       != nil) m_icon       = MCValueRetain(p_ref.m_icon);
    if (p_ref.m_image_data != nil) m_image_data = MCValueRetain(p_ref.m_image_data);
}

MCToolbarItem::~MCToolbarItem()
{
    if (m_name       != nil) MCValueRelease(m_name);
    if (m_label      != nil) MCValueRelease(m_label);
    if (m_tooltip    != nil) MCValueRelease(m_tooltip);
    if (m_icon       != nil) MCValueRelease(m_icon);
    if (m_image_data != nil) MCValueRelease(m_image_data);
}

void MCToolbarItem::SetName(MCNameRef p_name)
{
    MCValueAssign(m_name, p_name);
}

void MCToolbarItem::SetLabel(MCStringRef p_label)
{
    MCValueAssign(m_label, p_label);
}

void MCToolbarItem::SetTooltip(MCStringRef p_tooltip)
{
    MCValueAssign(m_tooltip, p_tooltip);
}

void MCToolbarItem::SetIcon(MCStringRef p_icon)
{
    MCValueAssign(m_icon, p_icon);
    // Clear any previously cached image data; it will be re-populated by
    // SetImageData() when the caller resolves the new name as an engine image.
    if (m_image_data != nil)
    {
        MCValueRelease(m_image_data);
        m_image_data = nil;
    }
}

void MCToolbarItem::SetImageData(MCDataRef p_data)
{
    MCValueAssign(m_image_data, p_data);
}

IO_stat MCToolbarItem::save(IO_handle p_stream, uint32_t p_version)
{
    IO_stat t_stat;

    // name
    t_stat = IO_write_stringref_new(MCNameGetString(m_name), p_stream,
                                    p_version >= kMCStackFileFormatVersion_7_0);
    if (t_stat != IO_NORMAL) return t_stat;

    // label
    MCStringRef t_label = m_label ? m_label : kMCEmptyString;
    t_stat = IO_write_stringref_new(t_label, p_stream,
                                    p_version >= kMCStackFileFormatVersion_7_0);
    if (t_stat != IO_NORMAL) return t_stat;

    // tooltip
    MCStringRef t_tooltip = m_tooltip ? m_tooltip : kMCEmptyString;
    t_stat = IO_write_stringref_new(t_tooltip, p_stream,
                                    p_version >= kMCStackFileFormatVersion_7_0);
    if (t_stat != IO_NORMAL) return t_stat;

    // icon
    MCStringRef t_icon = m_icon ? m_icon : kMCEmptyString;
    t_stat = IO_write_stringref_new(t_icon, p_stream,
                                    p_version >= kMCStackFileFormatVersion_7_0);
    if (t_stat != IO_NORMAL) return t_stat;

    // enabled + style packed into one byte
    uint8_t t_flags = (m_enabled ? 1 : 0) | ((uint8_t)m_style << 1);
    t_stat = IO_write_uint1(t_flags, p_stream);
    return t_stat;
}

IO_stat MCToolbarItem::load(IO_handle p_stream, uint32_t p_version)
{
    IO_stat t_stat;

    // name
    MCAutoStringRef t_name_str;
    t_stat = IO_read_stringref_new(&t_name_str, p_stream,
                                   p_version >= kMCStackFileFormatVersion_7_0);
    if (t_stat != IO_NORMAL) return t_stat;
    MCNewAutoNameRef t_name;
    /* UNCHECKED */ MCNameCreate(*t_name_str, &t_name);
    SetName(*t_name);

    // label
    MCAutoStringRef t_label;
    t_stat = IO_read_stringref_new(&t_label, p_stream,
                                   p_version >= kMCStackFileFormatVersion_7_0);
    if (t_stat != IO_NORMAL) return t_stat;
    SetLabel(*t_label);

    // tooltip
    MCAutoStringRef t_tooltip;
    t_stat = IO_read_stringref_new(&t_tooltip, p_stream,
                                   p_version >= kMCStackFileFormatVersion_7_0);
    if (t_stat != IO_NORMAL) return t_stat;
    SetTooltip(*t_tooltip);

    // icon
    MCAutoStringRef t_icon;
    t_stat = IO_read_stringref_new(&t_icon, p_stream,
                                   p_version >= kMCStackFileFormatVersion_7_0);
    if (t_stat != IO_NORMAL) return t_stat;
    SetIcon(*t_icon);

    // enabled + style
    uint8_t t_flags;
    t_stat = IO_read_uint1(&t_flags, p_stream);
    if (t_stat != IO_NORMAL) return t_stat;
    m_enabled = (t_flags & 1) != 0;
    m_style   = (MCToolbarItemStyle)((t_flags >> 1) & 0x0F);

    return IO_NORMAL;
}

////////////////////////////////////////////////////////////////////////////////
// MCToolbar constructor / destructor

MCToolbar::MCToolbar()
    : m_items(nil), m_item_count(0),
      m_display_mode(kMCToolbarDisplayModeDefault),
      m_toolbar_visible(true),
      m_backend(nil)
{
}

MCToolbar::MCToolbar(const MCToolbar &p_ref)
    : MCControl(p_ref),
      m_item_count(p_ref.m_item_count),
      m_display_mode(p_ref.m_display_mode),
      m_toolbar_visible(p_ref.m_toolbar_visible),
      m_backend(nil)
{
    m_items = nil;
    if (m_item_count > 0)
    {
        // Allocate raw memory (zeroed); construct each item in-place using the
        // copy constructor so that MCValueRef strings are properly retained.
        /* UNCHECKED */ MCMemoryNewArray(m_item_count, m_items);
        for (uindex_t i = 0; i < m_item_count; i++)
            new (&m_items[i]) MCToolbarItem(p_ref.m_items[i]);
    }
}

MCToolbar::~MCToolbar()
{
    _destroyItems();
    if (m_backend != nil)
    {
        m_backend->Destroy();
        delete m_backend;
        m_backend = nil;
    }
}

////////////////////////////////////////////////////////////////////////////////
// MCObject virtuals

Chunk_term MCToolbar::gettype() const
{
    return CT_TOOLBAR;
}

const char *MCToolbar::gettypestring()
{
    return MCtoolbarstring;
}

bool MCToolbar::visit_self(MCObjectVisitor *p_visitor)
{
    return p_visitor->OnToolbar(this);
}

////////////////////////////////////////////////////////////////////////////////
// Lifecycle

void MCToolbar::open()
{
    MCControl::open();

    if (m_backend == nil)
        m_backend = _createBackend();

    if (m_backend != nil)
    {
        void *t_window = nil;
        // Get the native window handle from the owning stack
        MCStack *t_stack = getstack();
        if (t_stack != nil)
            t_window = t_stack->getwindow();

        m_backend->Create(t_window);
        m_backend->SetDisplayMode(m_display_mode);
        m_backend->SetVisible(m_toolbar_visible);
        // Re-resolve stack image data before syncing — m_image_data is not
        // saved to disk, so it must be rebuilt each time the stack is opened.
        _resolveItemImageData();
        _syncBackendItems();
    }
}

void MCToolbar::close()
{
    if (m_backend != nil)
    {
        m_backend->Destroy();
        delete m_backend;
        m_backend = nil;
    }
    MCControl::close();
}

////////////////////////////////////////////////////////////////////////////////
// Events — toolbar has no in-canvas content to interact with

Boolean MCToolbar::mfocus(int2 x, int2 y)
{
    return False;
}

void MCToolbar::munfocus()
{
}

Boolean MCToolbar::mdown(uint2 which)
{
    return False;
}

Boolean MCToolbar::mup(uint2 which, bool p_release)
{
    return False;
}

Boolean MCToolbar::kdown(MCStringRef p_string, KeySym p_key)
{
    return False;
}

Boolean MCToolbar::kup(MCStringRef p_string, KeySym p_key)
{
    return False;
}

////////////////////////////////////////////////////////////////////////////////
// Rendering — the toolbar is drawn by the platform; we draw nothing in canvas

void MCToolbar::draw(MCDC *dc, const MCRectangle &dirty,
                     bool p_isolated, bool p_sprite)
{
    // Platform backends own all rendering — nothing to do here.
}

MCControl *MCToolbar::clone(Boolean attach, Object_pos p, bool invisible)
{
    fprintf(stderr, "[MCToolbar::clone] enter attach=%d\n", (int)attach); fflush(stderr);
    MCToolbar *t_new = new (nothrow) MCToolbar(*this);
    if (attach)
        t_new->attach(p, invisible);
    fprintf(stderr, "[MCToolbar::clone] done\n"); fflush(stderr);
    return t_new;
}

////////////////////////////////////////////////////////////////////////////////
// Item management

bool MCToolbar::AddItem(MCNameRef p_name, MCStringRef p_label,
                        MCStringRef p_tooltip, MCStringRef p_icon,
                        MCToolbarItemStyle p_style)
{
    fprintf(stderr, "[MCToolbar::AddItem] name=%p backend=%p\n",
            (void*)p_name, (void*)m_backend);
    fflush(stderr);

    if (!MCMemoryResizeArray(m_item_count + 1, m_items, m_item_count))
        return false;

    MCToolbarItem *t_item = &m_items[m_item_count - 1];
    t_item->SetName(p_name);
    t_item->SetLabel(p_label);
    t_item->SetTooltip(p_tooltip);
    t_item->SetIcon(p_icon);
    t_item->SetEnabled(true);
    t_item->SetStyle(p_style);

    if (m_backend != nil)
        m_backend->AddItem(t_item);
    else
        fprintf(stderr, "[MCToolbar::AddItem] backend is nil — item NOT pushed to NSToolbar\n");
    fflush(stderr);

    return true;
}

bool MCToolbar::RemoveItem(MCNameRef p_name)
{
    for (uindex_t i = 0; i < m_item_count; i++)
    {
        if (MCNameIsEqualTo(m_items[i].GetName(), p_name,
                            kMCCompareCaseless))
        {
            if (m_backend != nil)
                m_backend->RemoveItem(p_name);

            // Destroy the item at position i (releases its MCValueRef members).
            m_items[i].~MCToolbarItem();

            // Shift remaining items down, transferring ownership correctly:
            // copy-construct at [j] from [j+1], then destroy [j+1].
            for (uindex_t j = i; j < m_item_count - 1; j++)
            {
                new (&m_items[j]) MCToolbarItem(m_items[j + 1]);
                m_items[j + 1].~MCToolbarItem();
            }

            MCMemoryResizeArray(m_item_count - 1, m_items, m_item_count);
            return true;
        }
    }
    return false;
}

MCToolbarItem *MCToolbar::FindItem(MCNameRef p_name)
{
    for (uindex_t i = 0; i < m_item_count; i++)
    {
        if (MCNameIsEqualTo(m_items[i].GetName(), p_name,
                            kMCCompareCaseless))
            return &m_items[i];
    }
    return nil;
}

////////////////////////////////////////////////////////////////////////////////
// Message dispatch — called by the platform backend on item click

void MCToolbar::itemClicked(MCNameRef p_item_name)
{
    // The toolbar is parented to the stack, not a card, so a bare
    // message_with_valueref_args would dispatch toolbar → stack, skipping the
    // current card entirely.  Send through the current card instead so the
    // message follows the normal card → stack path that scripts expect.
    MCStack *t_stack = getstack();
    MCCard  *t_card  = t_stack != nil ? t_stack->getcurcard() : nil;
    MCObject *t_target = t_card != nil ? (MCObject *)t_card : (MCObject *)this;
    t_target->message_with_valueref_args(MCM_toolbar_item_clicked,
                                         MCNameGetString(p_item_name));
}

////////////////////////////////////////////////////////////////////////////////
// Save / load

#define TOOLBAR_EXTRA_ITEMS       (1 << 0)
#define TOOLBAR_EXTRA_DISPLAYMODE (1 << 1)
#define TOOLBAR_EXTRA_VISIBLE     (1 << 2)

IO_stat MCToolbar::save(IO_handle stream, uint4 p_part,
                        bool p_force_ext, uint32_t p_version)
{
    IO_stat stat;

    if ((stat = IO_write_uint1(OT_TOOLBAR, stream)) != IO_NORMAL)
        return stat;

    bool t_has_extension = m_item_count > 0 ||
                           m_display_mode != kMCToolbarDisplayModeDefault ||
                           !m_toolbar_visible;

    if ((stat = MCObject::save(stream, p_part,
                               t_has_extension || p_force_ext,
                               p_version)) != IO_NORMAL)
        return stat;

    return stat;
}

IO_stat MCToolbar::extendedsave(MCObjectOutputStream& p_stream,
                                uint4 p_part, uint32_t p_version)
{
    uint32_t t_flags = 0;
    uint32_t t_size  = 0;

    if (m_item_count > 0)
    {
        t_flags |= TOOLBAR_EXTRA_ITEMS;
        t_size  += sizeof(uint16_t); // item count
        // Each item: name, label, tooltip, icon (all new-format strings) + 1 flags byte
        // We measure each one properly below
    }
    if (m_display_mode != kMCToolbarDisplayModeDefault)
    {
        t_flags |= TOOLBAR_EXTRA_DISPLAYMODE;
        t_size  += sizeof(uint8_t);
    }
    if (!m_toolbar_visible)
    {
        t_flags |= TOOLBAR_EXTRA_VISIBLE;
        t_size  += sizeof(uint8_t);
    }

    // Measure item strings
    if (t_flags & TOOLBAR_EXTRA_ITEMS)
    {
        for (uindex_t i = 0; i < m_item_count; i++)
        {
            t_size += p_stream.MeasureStringRefNew(
                          MCNameGetString(m_items[i].GetName()),
                          p_version >= kMCStackFileFormatVersion_7_0);

            MCStringRef t_label = m_items[i].GetLabel()
                                  ? m_items[i].GetLabel() : kMCEmptyString;
            t_size += p_stream.MeasureStringRefNew(t_label,
                          p_version >= kMCStackFileFormatVersion_7_0);

            MCStringRef t_tooltip = m_items[i].GetTooltip()
                                    ? m_items[i].GetTooltip() : kMCEmptyString;
            t_size += p_stream.MeasureStringRefNew(t_tooltip,
                          p_version >= kMCStackFileFormatVersion_7_0);

            MCStringRef t_icon = m_items[i].GetIcon()
                                 ? m_items[i].GetIcon() : kMCEmptyString;
            t_size += p_stream.MeasureStringRefNew(t_icon,
                          p_version >= kMCStackFileFormatVersion_7_0);

            t_size += sizeof(uint8_t); // flags byte (enabled + style)
        }
    }

    IO_stat t_stat;
    t_stat = p_stream.WriteTag(t_flags, t_size);

    if (t_stat == IO_NORMAL && (t_flags & TOOLBAR_EXTRA_ITEMS))
    {
        t_stat = p_stream.WriteU16((uint16_t)m_item_count);
        for (uindex_t i = 0; i < m_item_count && t_stat == IO_NORMAL; i++)
        {
            t_stat = p_stream.WriteStringRefNew(
                         MCNameGetString(m_items[i].GetName()),
                         p_version >= kMCStackFileFormatVersion_7_0);

            MCStringRef t_label = m_items[i].GetLabel()
                                  ? m_items[i].GetLabel() : kMCEmptyString;
            if (t_stat == IO_NORMAL)
                t_stat = p_stream.WriteStringRefNew(t_label,
                             p_version >= kMCStackFileFormatVersion_7_0);

            MCStringRef t_tooltip = m_items[i].GetTooltip()
                                    ? m_items[i].GetTooltip() : kMCEmptyString;
            if (t_stat == IO_NORMAL)
                t_stat = p_stream.WriteStringRefNew(t_tooltip,
                             p_version >= kMCStackFileFormatVersion_7_0);

            MCStringRef t_icon = m_items[i].GetIcon()
                                 ? m_items[i].GetIcon() : kMCEmptyString;
            if (t_stat == IO_NORMAL)
                t_stat = p_stream.WriteStringRefNew(t_icon,
                             p_version >= kMCStackFileFormatVersion_7_0);

            if (t_stat == IO_NORMAL)
            {
                uint8_t t_item_flags =
                    (m_items[i].GetEnabled() ? 1 : 0) |
                    ((uint8_t)m_items[i].GetStyle() << 1);
                t_stat = p_stream.WriteU8(t_item_flags);
            }
        }
    }

    if (t_stat == IO_NORMAL && (t_flags & TOOLBAR_EXTRA_DISPLAYMODE))
        t_stat = p_stream.WriteU8((uint8_t)m_display_mode);

    if (t_stat == IO_NORMAL && (t_flags & TOOLBAR_EXTRA_VISIBLE))
        t_stat = p_stream.WriteU8(m_toolbar_visible ? 1 : 0);

    if (t_stat == IO_NORMAL)
        t_stat = MCObject::extendedsave(p_stream, p_part, p_version);

    return t_stat;
}

IO_stat MCToolbar::load(IO_handle stream, uint32_t version)
{
    IO_stat stat;
    if ((stat = MCObject::load(stream, version)) != IO_NORMAL)
        return stat;
    return stat;
}

IO_stat MCToolbar::extendedload(MCObjectInputStream& p_stream,
                                uint32_t p_version, uint4 p_length)
{
    IO_stat t_stat = IO_NORMAL;

    if (p_length > 0)
    {
        uint4 t_flags, t_length, t_header_length;
        t_stat = checkloadstat(p_stream.ReadTag(t_flags, t_length,
                                                t_header_length));

        if (t_stat == IO_NORMAL)
            t_stat = checkloadstat(p_stream.Mark());

        if (t_stat == IO_NORMAL && (t_flags & TOOLBAR_EXTRA_ITEMS))
        {
            uint16_t t_count;
            t_stat = checkloadstat(p_stream.ReadU16(t_count));

            for (uint16_t i = 0; i < t_count && t_stat == IO_NORMAL; i++)
            {
                // name
                MCAutoStringRef t_name_str;
                t_stat = checkloadstat(p_stream.ReadStringRefNew(
                    &t_name_str,
                    p_version >= kMCStackFileFormatVersion_7_0));
                MCNewAutoNameRef t_name;
                if (t_stat == IO_NORMAL)
                    /* UNCHECKED */ MCNameCreate(*t_name_str, &t_name);

                // label
                MCAutoStringRef t_label;
                if (t_stat == IO_NORMAL)
                    t_stat = checkloadstat(p_stream.ReadStringRefNew(
                        &t_label,
                        p_version >= kMCStackFileFormatVersion_7_0));

                // tooltip
                MCAutoStringRef t_tooltip;
                if (t_stat == IO_NORMAL)
                    t_stat = checkloadstat(p_stream.ReadStringRefNew(
                        &t_tooltip,
                        p_version >= kMCStackFileFormatVersion_7_0));

                // icon
                MCAutoStringRef t_icon;
                if (t_stat == IO_NORMAL)
                    t_stat = checkloadstat(p_stream.ReadStringRefNew(
                        &t_icon,
                        p_version >= kMCStackFileFormatVersion_7_0));

                // flags byte
                uint8_t t_item_flags = 0;
                if (t_stat == IO_NORMAL)
                    t_stat = checkloadstat(p_stream.ReadU8(t_item_flags));

                if (t_stat == IO_NORMAL)
                {
                    bool t_enabled = (t_item_flags & 1) != 0;
                    MCToolbarItemStyle t_style =
                        (MCToolbarItemStyle)((t_item_flags >> 1) & 0x0F);
                    AddItem(*t_name, *t_label, *t_tooltip, *t_icon, t_style);
                    MCToolbarItem *t_item = FindItem(*t_name);
                    if (t_item != nil)
                        t_item->SetEnabled(t_enabled);
                }
            }
        }

        if (t_stat == IO_NORMAL && (t_flags & TOOLBAR_EXTRA_DISPLAYMODE))
        {
            uint8_t t_value;
            t_stat = checkloadstat(p_stream.ReadU8(t_value));
            if (t_stat == IO_NORMAL)
                m_display_mode = (MCToolbarDisplayMode)t_value;
        }

        if (t_stat == IO_NORMAL && (t_flags & TOOLBAR_EXTRA_VISIBLE))
        {
            uint8_t t_value;
            t_stat = checkloadstat(p_stream.ReadU8(t_value));
            if (t_stat == IO_NORMAL)
                m_toolbar_visible = (t_value != 0);
        }

        if (t_stat == IO_NORMAL)
            t_stat = checkloadstat(p_stream.Skip(t_length));

        if (t_stat == IO_NORMAL)
            p_length -= t_length + t_header_length;
    }

    if (t_stat == IO_NORMAL)
        t_stat = MCObject::extendedload(p_stream, p_version, p_length);

    return t_stat;
}

////////////////////////////////////////////////////////////////////////////////
// Layout helpers

int32_t MCToolbar::getToolbarTopY()
{
    MCStack *t_stack = getstack();
    if (t_stack == nil || !t_stack->hasmenubar())
        return 0;

    MCCard *t_card = t_stack->getcurcard();
    if (t_card == nil)
        return 0;

    // Find the menu bar group on the current card — same lookup used by
    // getnextscroll() on macOS.
    MCControl *t_mbptr = t_card->getchild(
        CT_EXPRESSION,
        MCNameGetString(t_stack->getmenubar()),
        CT_GROUP, CT_UNDEFINED);

    if (t_mbptr == nil || !t_mbptr->getopened() || !t_mbptr->isvisible())
        return 0;

    const MCRectangle &r = t_mbptr->getrect();
    return (int32_t)(r.y + r.height);
}

////////////////////////////////////////////////////////////////////////////////
// Private helpers

void MCToolbar::_destroyItems()
{
    if (m_items != nil)
    {
        for (uindex_t i = 0; i < m_item_count; i++)
            m_items[i].~MCToolbarItem();
        MCMemoryDeleteArray(m_items);
        m_items = nil;
        m_item_count = 0;
    }
}

void MCToolbar::_resolveItemImageData()
{
    // Walk every item and (re-)populate m_image_data by looking up the icon
    // name as a stack MCImage object.  This is called from open() so that
    // image data that was cached at script time is restored after the stack
    // is reloaded from disk (the PNG bytes are not written to the stack file —
    // only the icon name is saved; the MCImage object already holds the data).
    //
    // Uses a default MCExecContext (no associated handler/object) which is
    // sufficient for MCImage::GetText — that method only uses the context for
    // error reporting, not for data retrieval.
    MCExecContext t_ctxt;

    for (uindex_t i = 0; i < m_item_count; i++)
    {
        MCToolbarItem *t_item = &m_items[i];
        MCStringRef t_icon = t_item->GetIcon();
        if (MCStringIsEmpty(t_icon))
            continue;

        MCImage *t_image = resolveimagename(t_icon);
        if (t_image == nil)
            continue;

        MCAutoDataRef t_data;
        t_image->GetText(t_ctxt, &t_data);
        if (*t_data != nil && !MCDataIsEmpty(*t_data))
            t_item->SetImageData(*t_data);
        else
            t_item->SetImageData(nil);  // clear stale cache if image is now empty
    }
}

void MCToolbar::_syncBackendItems()
{
    fprintf(stderr, "[MCToolbar::_syncBackendItems] enter backend=%p item_count=%u\n",
            (void*)m_backend, (unsigned)m_item_count); fflush(stderr);
    if (m_backend == nil)
        return;
    fprintf(stderr, "[MCToolbar::_syncBackendItems] calling ClearItems\n"); fflush(stderr);
    m_backend->ClearItems();
    fprintf(stderr, "[MCToolbar::_syncBackendItems] ClearItems done\n"); fflush(stderr);
    for (uindex_t i = 0; i < m_item_count; i++)
    {
        fprintf(stderr, "[MCToolbar::_syncBackendItems] AddItem %u\n", (unsigned)i); fflush(stderr);
        m_backend->AddItem(&m_items[i]);
    }
    fprintf(stderr, "[MCToolbar::_syncBackendItems] done\n"); fflush(stderr);
}

MCToolbarBackend *MCToolbar::_createBackend()
{
    return MCToolbarCreatePlatformBackend(this);
}
