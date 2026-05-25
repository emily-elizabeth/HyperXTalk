//
// MCToolbar and MCToolbarItem class declarations
//
#ifndef TOOLBAR_H
#define TOOLBAR_H

#include "mccontrol.h"

////////////////////////////////////////////////////////////////////////////////
// Item style constants

enum MCToolbarItemStyle
{
    kMCToolbarItemStyleButton    = 0,  // clickable button (default)
    kMCToolbarItemStyleSeparator = 1,  // fixed separator line
    kMCToolbarItemStyleSpace     = 2,  // fixed space
    kMCToolbarItemStyleFlexSpace = 3,  // flexible space (expands to fill)
};

// Display mode constants
enum MCToolbarDisplayMode
{
    kMCToolbarDisplayModeDefault      = 0,
    kMCToolbarDisplayModeIconAndLabel = 1,
    kMCToolbarDisplayModeIconOnly     = 2,
    kMCToolbarDisplayModeLabelOnly    = 3,
};

////////////////////////////////////////////////////////////////////////////////
// MCToolbarItem — a single item in a toolbar

class MCToolbarItem
{
public:
    MCToolbarItem();
    MCToolbarItem(const MCToolbarItem &p_ref);
    ~MCToolbarItem();

    // Identity
    MCNameRef         GetName()    const { return m_name; }
    void              SetName(MCNameRef p_name);

    // Display
    MCStringRef       GetLabel()   const { return m_label; }
    void              SetLabel(MCStringRef p_label);

    MCStringRef       GetTooltip() const { return m_tooltip; }
    void              SetTooltip(MCStringRef p_tooltip);

    // The icon name.  When the name matches a stack image object the raw image
    // data (PNG bytes from the image's "text" property) is also cached in
    // m_image_data so the platform backend can create a native image without
    // needing to reach back into the engine object hierarchy.
    MCStringRef       GetIcon()      const { return m_icon; }
    void              SetIcon(MCStringRef p_icon);

    // Pre-resolved PNG bytes for stack images; nil when the icon is an SF
    // Symbol, bundle image, or file path.
    MCDataRef         GetImageData() const { return m_image_data; }
    void              SetImageData(MCDataRef p_data);

    // State
    bool              GetEnabled() const { return m_enabled; }
    void              SetEnabled(bool p_enabled) { m_enabled = p_enabled; }

    MCToolbarItemStyle GetStyle()  const { return m_style; }
    void              SetStyle(MCToolbarItemStyle p_style) { m_style = p_style; }

    // Persistence
    IO_stat           load(IO_handle p_stream, uint32_t p_version);
    IO_stat           save(IO_handle p_stream, uint32_t p_version);

private:
    MCNameRef          m_name;
    MCStringRef        m_label;
    MCStringRef        m_tooltip;
    MCStringRef        m_icon;
    MCDataRef          m_image_data;  // cached PNG bytes from a stack MCImage, or nil
    bool               m_enabled;
    MCToolbarItemStyle m_style;
};

////////////////////////////////////////////////////////////////////////////////
// Platform backend interface — implemented per platform

class MCToolbarBackend
{
public:
    virtual ~MCToolbarBackend() {}

    // Lifecycle
    virtual void Create(void *p_window_handle) = 0;
    virtual void Destroy() = 0;

    // Items
    virtual void AddItem(const MCToolbarItem *p_item) = 0;
    virtual void RemoveItem(MCNameRef p_name) = 0;
    virtual void UpdateItem(const MCToolbarItem *p_item) = 0;
    virtual void ClearItems() = 0;

    // Toolbar state
    virtual void SetDisplayMode(MCToolbarDisplayMode p_mode) = 0;
    virtual void SetVisible(bool p_visible) = 0;
    virtual bool GetVisible() = 0;

    // Called by the platform backend when an item is clicked — routes to MCToolbar
    // via itemClicked() below.
};

////////////////////////////////////////////////////////////////////////////////
// MCToolbar — first-class engine object

typedef MCObjectProxy<MCToolbar>::Handle MCToolbarHandle;

class MCToolbar : public MCControl, public MCMixinObjectHandle<MCToolbar>
{
public:
    enum { kObjectType = CT_TOOLBAR };
    using MCMixinObjectHandle<MCToolbar>::GetHandle;

private:
    // Items
    MCToolbarItem  *m_items;
    uindex_t        m_item_count;

    // Display state
    MCToolbarDisplayMode m_display_mode;
    bool                 m_toolbar_visible;

    // Platform backend (null until open())
    MCToolbarBackend    *m_backend;

    static MCPropertyInfo        kProperties[];
    static MCObjectPropertyTable kPropertyTable;

public:
    MCToolbar();
    MCToolbar(const MCToolbar &p_ref);
    virtual ~MCToolbar();

    //--------------------------------------------------------------------------
    // MCObject virtuals

    virtual Chunk_term gettype() const;
    virtual const char *gettypestring();

    virtual const MCObjectPropertyTable *getpropertytable(void) const
        { return &kPropertyTable; }

    virtual bool visit_self(MCObjectVisitor *p_visitor);

    //--------------------------------------------------------------------------
    // MCControl virtuals — lifecycle

    virtual void open();
    virtual void close();

    //--------------------------------------------------------------------------
    // MCControl virtuals — events

    virtual Boolean mfocus(int2 x, int2 y);
    virtual void    munfocus();
    virtual Boolean mdown(uint2 which);
    virtual Boolean mup(uint2 which, bool p_release);
    virtual Boolean kdown(MCStringRef p_string, KeySym p_key);
    virtual Boolean kup(MCStringRef p_string, KeySym p_key);

    //--------------------------------------------------------------------------
    // MCControl virtuals — rendering

    virtual void draw(MCDC *dc, const MCRectangle &dirty,
                      bool p_isolated, bool p_sprite);
    virtual MCControl *clone(Boolean attach, Object_pos p, bool invisible);

    //--------------------------------------------------------------------------
    // Persistence

    IO_stat load(IO_handle stream, uint32_t version);
    IO_stat extendedload(MCObjectInputStream& p_stream,
                         uint32_t version, uint4 p_length);
    IO_stat save(IO_handle stream, uint4 p_part,
                 bool p_force_ext, uint32_t p_version);
    IO_stat extendedsave(MCObjectOutputStream& p_stream,
                         uint4 p_part, uint32_t p_version);

    //--------------------------------------------------------------------------
    // Item management (called from script via exec-interface-toolbar)

    bool        AddItem(MCNameRef p_name, MCStringRef p_label,
                        MCStringRef p_tooltip, MCStringRef p_icon,
                        MCToolbarItemStyle p_style);
    bool        RemoveItem(MCNameRef p_name);
    MCToolbarItem *FindItem(MCNameRef p_name);
    uindex_t    GetItemCount() const { return m_item_count; }
    MCToolbarItem *GetItem(uindex_t p_index) { return &m_items[p_index]; }

    //--------------------------------------------------------------------------
    // Property accessors (called by exec-interface-toolbar)

    void GetDisplayMode(MCExecContext& ctxt, intenum_t& r_mode);
    void SetDisplayMode(MCExecContext& ctxt, intenum_t p_mode);

    void GetToolbarVisible(MCExecContext& ctxt, bool& r_visible);
    void SetToolbarVisible(MCExecContext& ctxt, bool p_visible);

    void GetItemNames(MCExecContext& ctxt, MCStringRef& r_names);
    // Setting itemNames reorders/prunes the item list:
    // items not in the new list are removed; order is updated to match.
    void SetItemNames(MCExecContext& ctxt, MCStringRef p_names);

    // Per-item property accessors
    void GetItemLabel(MCExecContext& ctxt, MCNameRef p_item,
                      MCStringRef& r_label);
    void SetItemLabel(MCExecContext& ctxt, MCNameRef p_item,
                      MCStringRef p_label);

    void GetItemTooltip(MCExecContext& ctxt, MCNameRef p_item,
                        MCStringRef& r_tooltip);
    void SetItemTooltip(MCExecContext& ctxt, MCNameRef p_item,
                        MCStringRef p_tooltip);

    void GetItemEnabled(MCExecContext& ctxt, MCNameRef p_item,
                        bool& r_enabled);
    void SetItemEnabled(MCExecContext& ctxt, MCNameRef p_item,
                        bool p_enabled);

    void GetItemIcon(MCExecContext& ctxt, MCNameRef p_item,
                     MCStringRef& r_icon);
    void SetItemIcon(MCExecContext& ctxt, MCNameRef p_item,
                     MCStringRef p_icon);

    void GetItemStyle(MCExecContext& ctxt, MCNameRef p_item,
                      MCStringRef& r_style);
    void SetItemStyle(MCExecContext& ctxt, MCNameRef p_item,
                      MCStringRef p_style);

    //--------------------------------------------------------------------------
    // Called by the platform backend when an item is clicked

    void itemClicked(MCNameRef p_item_name);

    // Returns the y pixel offset (in client-area coordinates) at which the
    // platform toolbar should appear.  If the stack has an in-window menu bar
    // group this is the group's bottom edge; otherwise it is 0.
    int32_t getToolbarTopY();

private:
    void _destroyItems();
    void _syncBackendItems();
    // Re-resolves each item's icon name against the stack's MCImage objects and
    // caches the PNG bytes in MCToolbarItem::m_image_data.  Called from open()
    // so that image data is available when the stack is reopened from disk.
    void _resolveItemImageData();
    MCToolbarBackend *_createBackend();
};

////////////////////////////////////////////////////////////////////////////////
// Platform backend factory — defined in mac-toolbar.mm / w32-toolbar.cpp /
// lnx-toolbar.cpp

MCToolbarBackend *MCToolbarCreatePlatformBackend(MCToolbar *p_owner);

#endif // TOOLBAR_H
