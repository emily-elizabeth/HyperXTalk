//
// macOS NSToolbar backend for MCToolbar.
// Ported from the org.openxtalk.nstoolbar extension (Emily-Elizabeth Howard).
//

#import <Cocoa/Cocoa.h>

#include "prefix.h"
#include "toolbar.h"
// platform.h / platform-internal.h give us the full MCPlatformWindow
// definition that mac-internal.h (MCMacPlatformWindow) inherits from.
#include "platform.h"
#include "platform-internal.h"
#include "mac-internal.h"


////////////////////////////////////////////////////////////////////////////////
// Forward declare C++ backend so ObjC delegate can hold a pointer to it.
// The @implementation that calls OnItemClicked() is split into a category
// defined AFTER the complete C++ class body.

class MCToolbarMacBackend;

////////////////////////////////////////////////////////////////////////////////
// NSToolbarDelegate interface

@interface MCNSToolbarDelegate : NSObject <NSToolbarDelegate>
@property (nonatomic, assign) MCToolbarMacBackend *backend;
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSMutableDictionary *> *itemMeta;
@property (nonatomic, strong) NSMutableArray<NSString *>  *itemOrder;
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSImage *> *itemImages;
@end

////////////////////////////////////////////////////////////////////////////////
// Icon resolution helpers
//
// These are defined before @implementation so the delegate callback can call
// them without a forward declaration.
//
// Resolution cascade for MCToolbarResolveIcon (string-based):
//
//  Explicit prefix (takes priority, no fallback):
//    "sf:<name>"       → SF Symbol (macOS 11+)
//    "file:<path>"     → image file at <path>
//    "engine:<name>"   → (reserved) future engine MCImage support; currently
//                        falls through to the named-image search below
//
//  Auto-detect (tried in order when no prefix is present):
//    1. SF Symbol (macOS 11+, silent failure)
//    2. [NSImage imageNamed:]  — covers the app's asset catalog, bundle
//       Resources folder, and all built-in NSImage* system-image names
//    3. File path (only when the string begins with '/' or '~')
//
// MCToolbarResolveIconFromData is used when the C++ layer has already
// extracted raw PNG bytes from a stack MCImage object.

// Resolve from pre-extracted PNG bytes (from a stack MCImage's "text" property).
static NSImage *MCToolbarResolveIconFromData(const void *p_bytes, uindex_t p_length)
{
    if (!p_bytes || p_length == 0)
        return nil;
    NSData *nsdata = [NSData dataWithBytes:p_bytes length:(NSUInteger)p_length];
    return [[NSImage alloc] initWithData:nsdata];
}

static NSImage *MCToolbarResolveIcon(NSString *iconName, NSString *label)
{
    if (!iconName || iconName.length == 0)
        return nil;

    // ── Explicit prefix ──────────────────────────────────────────────────────
    if ([iconName hasPrefix:@"sf:"])
    {
        NSString *sym = [iconName substringFromIndex:3];
        if (@available(macOS 11.0, *))
            return [NSImage imageWithSystemSymbolName:sym
                              accessibilityDescription:label ?: @""];
        return nil;  // SF Symbols require macOS 11
    }

    if ([iconName hasPrefix:@"file:"])
    {
        NSString *path = [[iconName substringFromIndex:5]
                            stringByExpandingTildeInPath];
        return [[NSImage alloc] initWithContentsOfFile:path];
    }

    if ([iconName hasPrefix:@"engine:"])
    {
        // Reserved for future engine MCImage object support.
        // For now, strip the prefix and fall through to the named-image search.
        iconName = [iconName substringFromIndex:7];
    }

    // ── Auto-detect ──────────────────────────────────────────────────────────

    // 1. Try as an SF Symbol (macOS 11+).
    NSImage *img = nil;
    if (@available(macOS 11.0, *))
        img = [NSImage imageWithSystemSymbolName:iconName
                          accessibilityDescription:label ?: @""];
    if (img) return img;

    // 2. Try as a named image: app asset catalog, bundle Resources, NSImage
    //    system names (NSImageNameApplicationIcon, etc.), and any image that
    //    has been cached with +[NSImage setName:] elsewhere in the process.
    img = [NSImage imageNamed:iconName];
    if (img) return img;

    // 3. Try as a file-system path when the string looks like one.
    if ([iconName hasPrefix:@"/"] || [iconName hasPrefix:@"~"])
    {
        NSString *expanded = [iconName stringByExpandingTildeInPath];
        img = [[NSImage alloc] initWithContentsOfFile:expanded];
    }

    return img;
}

////////////////////////////////////////////////////////////////////////////////
// NSToolbarDelegate implementation — all delegate methods EXCEPT
// toolbarItemClicked:, which is in the (MCBackendActions) category below
// so that it can see the complete MCToolbarMacBackend definition.

@implementation MCNSToolbarDelegate

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        // Use alloc/init (not the autoreleasing convenience constructors) so
        // that these ivars are +1 retained regardless of whether the caller
        // is using ARC.  Direct ivar assignment bypasses the strong property
        // setter in MRR mode, so the autoreleased form would be released the
        // moment the caller's autorelease pool drains.
        _itemMeta   = [[NSMutableDictionary alloc] init];
        _itemOrder  = [[NSMutableArray alloc] init];
        _itemImages = [[NSMutableDictionary alloc] init];
    }
    return self;
}

#if !__has_feature(objc_arc)
- (void)dealloc
{
    [_itemMeta   release];
    [_itemOrder  release];
    [_itemImages release];
    [super dealloc];
}
#endif

- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
    return [_itemOrder copy];
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    NSMutableArray *allowed = [_itemOrder mutableCopy];
    [allowed addObject:NSToolbarFlexibleSpaceItemIdentifier];
    [allowed addObject:NSToolbarSpaceItemIdentifier];
    return [allowed copy];
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
     itemForItemIdentifier:(NSString *)itemIdentifier
 willBeInsertedIntoToolbar:(BOOL)flag
{
    // Pass through system items
    if ([itemIdentifier hasPrefix:@"NSToolbar"])
        return nil;

    NSMutableDictionary *meta = _itemMeta[itemIdentifier];
    if (!meta)
        return nil;

    NSToolbarItem *item = [[NSToolbarItem alloc]
                            initWithItemIdentifier:itemIdentifier];

    NSString *label = meta[@"label"] ?: itemIdentifier;
    item.label        = label;
    item.paletteLabel = label;
    item.toolTip      = meta[@"tooltip"] ?: @"";

    NSImage *img = _itemImages[itemIdentifier];
    if (!img)
        img = MCToolbarResolveIcon(meta[@"iconName"], label);
    if (img)
        item.image = img;

    item.target        = self;
    item.action        = @selector(toolbarItemClicked:);
    item.autovalidates = NO;
    item.enabled       = [meta[@"enabled"] boolValue];

    return item;
}

@end

////////////////////////////////////////////////////////////////////////////////
// Debug logging — NSLog values are redacted as <private> by the macOS privacy
// subsystem, so write directly to a temp file instead.

static void MCToolbarDebugLog(const char *fmt, ...) __attribute__((format(printf,1,2)));
static void MCToolbarDebugLog(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

////////////////////////////////////////////////////////////////////////////////
// C++ backend implementation

class MCToolbarMacBackend : public MCToolbarBackend
{
public:
    MCToolbarMacBackend(MCToolbar *p_owner)
        : m_owner(p_owner), m_toolbar(nil), m_delegate(nil), m_window(nil)
    {
    }

    ~MCToolbarMacBackend() override
    {
    }

    void Create(void *p_window_handle) override
    {
        @autoreleasepool
        {
            // Use the owner object's ID as the toolbar identifier so it's
            // unique per stack.
            char t_id_buf[32];
            snprintf(t_id_buf, sizeof(t_id_buf), "mctoolbar_%p", (void *)m_owner);
            NSString *t_ident = [NSString stringWithUTF8String:t_id_buf];

            m_delegate = [[MCNSToolbarDelegate alloc] init];
            m_delegate.backend = this;

            m_toolbar = [[NSToolbar alloc] initWithIdentifier:t_ident];
            m_toolbar.delegate = m_delegate;
            m_toolbar.allowsUserCustomization = YES;

            MCToolbarDebugLog("--- Create() p_window_handle=%p ---", p_window_handle);
            // p_window_handle is MCPlatformWindowRef (= MCMacPlatformWindow* on
            // macOS), NOT a raw NSWindow*.  Call GetHandle() to obtain the
            // underlying NSWindow that AppKit owns.
            MCMacPlatformWindow *t_platform_win =
                (MCMacPlatformWindow *)p_window_handle;
            m_window = t_platform_win
                       ? (NSWindow *)t_platform_win->GetHandle()
                       : nil;
            if (m_window)
            {
                // Attaching a toolbar shrinks the content area in-place, which
                // causes the window contents to creep down by the toolbar height
                // on every open/close cycle.  Capture the content rect first,
                // then restore it after attaching so the content area stays put.
                NSRect t_frame_before = m_window.frame;
                NSRect t_content = [m_window
                                    contentRectForFrameRect:t_frame_before];
                MCToolbarDebugLog("[MCToolbar] Create: frame_before={%.0f,%.0f,%.0f,%.0f} content={%.0f,%.0f,%.0f,%.0f}",
                      t_frame_before.origin.x, t_frame_before.origin.y,
                      t_frame_before.size.width, t_frame_before.size.height,
                      t_content.origin.x, t_content.origin.y,
                      t_content.size.width, t_content.size.height);
                m_window.toolbar = m_toolbar;
                NSRect t_frame_after_attach = m_window.frame;
                NSRect t_content_after_attach = [m_window contentRectForFrameRect:t_frame_after_attach];
                MCToolbarDebugLog("[MCToolbar] Create: after-attach frame={%.0f,%.0f,%.0f,%.0f} content={%.0f,%.0f,%.0f,%.0f}",
                      t_frame_after_attach.origin.x, t_frame_after_attach.origin.y,
                      t_frame_after_attach.size.width, t_frame_after_attach.size.height,
                      t_content_after_attach.origin.x, t_content_after_attach.origin.y,
                      t_content_after_attach.size.width, t_content_after_attach.size.height);
                NSRect t_new_frame = [m_window
                                      frameRectForContentRect:t_content];
                MCToolbarDebugLog("[MCToolbar] Create: t_new_frame={%.0f,%.0f,%.0f,%.0f} (delta h=%.0f)",
                      t_new_frame.origin.x, t_new_frame.origin.y,
                      t_new_frame.size.width, t_new_frame.size.height,
                      t_new_frame.size.height - t_frame_after_attach.size.height);
                [m_window setFrame:t_new_frame display:NO];
                NSRect t_frame_final = m_window.frame;
                MCToolbarDebugLog("[MCToolbar] Create: frame_final={%.0f,%.0f,%.0f,%.0f}",
                      t_frame_final.origin.x, t_frame_final.origin.y,
                      t_frame_final.size.width, t_frame_final.size.height);
            }
        }
    }

    void Destroy() override
    {
        @autoreleasepool
        {
            MCToolbarDebugLog("--- Destroy() m_toolbar=%p m_window=%p ---",
                              (void *)m_toolbar, (void *)m_window);
            if (m_toolbar && m_window)
            {
                // Capture the content rect WITH the toolbar attached.  This is
                // the stack's true usable area (below the toolbar).  We must do
                // this BEFORE detaching, because window.toolbar = nil expands
                // the content area in-place (frame stays, content grows by
                // toolbar height), which fires ProcessDidResize → m_content
                // is saved with the bloated value → on the next open the frame
                // is set too large before the toolbar is re-attached, causing
                // the content to creep down by one toolbar height per cycle.
                NSRect t_frame_before = m_window.frame;
                NSRect t_content = [m_window contentRectForFrameRect:t_frame_before];
                MCToolbarDebugLog("[MCToolbar] Destroy: frame_before={%.0f,%.0f,%.0f,%.0f} content={%.0f,%.0f,%.0f,%.0f}",
                      t_frame_before.origin.x, t_frame_before.origin.y,
                      t_frame_before.size.width, t_frame_before.size.height,
                      t_content.origin.x, t_content.origin.y,
                      t_content.size.width, t_content.size.height);

                m_toolbar.delegate = nil;
                if ([m_window.toolbar isEqual:m_toolbar])
                    m_window.toolbar = nil;
#if !__has_feature(objc_arc)
                [m_toolbar release];
#endif
                m_toolbar = nil;

                NSRect t_frame_after_remove = m_window.frame;
                MCToolbarDebugLog("[MCToolbar] Destroy: after-remove frame={%.0f,%.0f,%.0f,%.0f}",
                      t_frame_after_remove.origin.x, t_frame_after_remove.origin.y,
                      t_frame_after_remove.size.width, t_frame_after_remove.size.height);

                // Shrink the frame so the content area stays at exactly the
                // same rect it had while the toolbar was attached.  This fires
                // ProcessDidResize with the correct (small) content, which
                // saves the right m_content for the engine.
                NSRect t_new_frame = [m_window frameRectForContentRect:t_content];
                MCToolbarDebugLog("[MCToolbar] Destroy: t_new_frame={%.0f,%.0f,%.0f,%.0f} (delta h=%.0f)",
                      t_new_frame.origin.x, t_new_frame.origin.y,
                      t_new_frame.size.width, t_new_frame.size.height,
                      t_new_frame.size.height - t_frame_after_remove.size.height);
                [m_window setFrame:t_new_frame display:NO];
                NSRect t_frame_final = m_window.frame;
                MCToolbarDebugLog("[MCToolbar] Destroy: frame_final={%.0f,%.0f,%.0f,%.0f}",
                      t_frame_final.origin.x, t_frame_final.origin.y,
                      t_frame_final.size.width, t_frame_final.size.height);
            }
            m_window   = nil;
#if !__has_feature(objc_arc)
            // In MRR mode __strong is a no-op, so nil-assignment below does not
            // release.  Balance the alloc/init retain from Create() explicitly.
            [m_delegate release];
#endif
            m_delegate = nil;
        }
    }

    void AddItem(const MCToolbarItem *p_item) override
    {
        @autoreleasepool
        {
            NSString *t_ident = _nameToNSString(p_item->GetName());
            MCToolbarDebugLog("[MCToolbar] AddItem ident=%s toolbar=%p",
                              t_ident.UTF8String, (void*)m_toolbar);

            NSString *t_label   = _stringRefToNSString(p_item->GetLabel());
            NSString *t_iconName = _stringRefToNSString(p_item->GetIcon());

            NSMutableDictionary *meta = [NSMutableDictionary dictionary];
            meta[@"label"]    = t_label;
            meta[@"tooltip"]  = _stringRefToNSString(p_item->GetTooltip());
            meta[@"iconName"] = t_iconName;
            meta[@"enabled"]  = @(p_item->GetEnabled());

            // Pre-cache the image so the delegate callback doesn't need to
            // re-resolve it.  Stack image data (PNG bytes from GetImageData)
            // takes priority over the string-based resolution cascade.
            MCDataRef t_img_data = p_item->GetImageData();
            NSImage *t_preresolved = nil;
            if (t_img_data != nil && !MCDataIsEmpty(t_img_data))
            {
                t_preresolved = MCToolbarResolveIconFromData(
                    MCDataGetBytePtr(t_img_data),
                    MCDataGetLength(t_img_data));
            }
            if (!t_preresolved)
                t_preresolved = MCToolbarResolveIcon(t_iconName, t_label);
            if (t_preresolved)
                m_delegate.itemImages[t_ident] = t_preresolved;

            m_delegate.itemMeta[t_ident]  = meta;
            [m_delegate.itemOrder addObject:t_ident];

            MCToolbarDebugLog("[MCToolbar] AddItem meta stored, inserting into toolbar (items=%lu)",
                              (unsigned long)m_toolbar.items.count);
            if (m_toolbar)
            {
                NSInteger t_index = (NSInteger)m_toolbar.items.count;
                [m_toolbar insertItemWithItemIdentifier:t_ident atIndex:t_index];
                MCToolbarDebugLog("[MCToolbar] AddItem inserted, toolbar now has %lu items",
                                  (unsigned long)m_toolbar.items.count);
            }
        }
    }

    void RemoveItem(MCNameRef p_name) override
    {
        @autoreleasepool
        {
            NSString *t_ident = _nameToNSString(p_name);

            if (m_toolbar)
            {
                NSArray *items = m_toolbar.items;
                for (NSInteger i = (NSInteger)items.count - 1; i >= 0; i--)
                {
                    if ([((NSToolbarItem *)items[i]).itemIdentifier
                            isEqualToString:t_ident])
                    {
                        [m_toolbar removeItemAtIndex:i];
                        break;
                    }
                }
            }

            [m_delegate.itemMeta   removeObjectForKey:t_ident];
            [m_delegate.itemOrder  removeObject:t_ident];
            [m_delegate.itemImages removeObjectForKey:t_ident];
        }
    }

    void UpdateItem(const MCToolbarItem *p_item) override
    {
        @autoreleasepool
        {
            NSString *t_ident = _nameToNSString(p_item->GetName());

            NSMutableDictionary *meta = m_delegate.itemMeta[t_ident];
            if (!meta)
                return;

            meta[@"label"]   = _stringRefToNSString(p_item->GetLabel());
            meta[@"tooltip"] = _stringRefToNSString(p_item->GetTooltip());
            meta[@"enabled"] = @(p_item->GetEnabled());

            // Push live updates to existing toolbar items
            if (m_toolbar)
            {
                for (NSToolbarItem *item in m_toolbar.items)
                {
                    if ([item.itemIdentifier isEqualToString:t_ident])
                    {
                        NSString *lbl = meta[@"label"];
                        item.label        = lbl;
                        item.paletteLabel = lbl;
                        item.toolTip      = meta[@"tooltip"] ?: @"";
                        item.enabled      = p_item->GetEnabled();
                        break;
                    }
                }
            }

            // Icon update: reload image if icon name changed.
            NSString *t_iconName = _stringRefToNSString(p_item->GetIcon());
            meta[@"iconName"] = t_iconName;  // keep meta in sync

            // Stack image data (pre-resolved PNG bytes) takes priority.
            NSImage *img = nil;
            MCDataRef t_img_data = p_item->GetImageData();
            if (t_img_data != nil && !MCDataIsEmpty(t_img_data))
            {
                img = MCToolbarResolveIconFromData(
                    MCDataGetBytePtr(t_img_data),
                    MCDataGetLength(t_img_data));
            }
            if (!img)
                img = MCToolbarResolveIcon(t_iconName, meta[@"label"]);
            if (img)
            {
                m_delegate.itemImages[t_ident] = img;
                if (m_toolbar)
                {
                    for (NSToolbarItem *item in m_toolbar.items)
                    {
                        if ([item.itemIdentifier isEqualToString:t_ident])
                        {
                            item.image = img;
                            break;
                        }
                    }
                }
            }
            else
            {
                // Icon was cleared or couldn't be resolved — remove cached image
                // so the item falls back to label-only display.
                [m_delegate.itemImages removeObjectForKey:t_ident];
                if (m_toolbar)
                {
                    for (NSToolbarItem *item in m_toolbar.items)
                    {
                        if ([item.itemIdentifier isEqualToString:t_ident])
                        {
                            item.image = nil;
                            break;
                        }
                    }
                }
            }
        }
    }

    void ClearItems() override
    {
        MCToolbarDebugLog("[MCToolbar] ClearItems m_toolbar=%p m_delegate=%p",
                          (void*)m_toolbar, (void*)m_delegate);
        @autoreleasepool
        {
            if (m_toolbar)
            {
                MCToolbarDebugLog("[MCToolbar] ClearItems removing %lu toolbar items",
                                  (unsigned long)m_toolbar.items.count);
                while (m_toolbar.items.count > 0)
                    [m_toolbar removeItemAtIndex:0];
                MCToolbarDebugLog("[MCToolbar] ClearItems toolbar items cleared");
            }
            MCToolbarDebugLog("[MCToolbar] ClearItems clearing delegate dicts "
                              "meta=%p order=%p images=%p",
                              (void*)m_delegate.itemMeta,
                              (void*)m_delegate.itemOrder,
                              (void*)m_delegate.itemImages);
            [m_delegate.itemMeta   removeAllObjects];
            MCToolbarDebugLog("[MCToolbar] ClearItems itemMeta cleared");
            [m_delegate.itemOrder  removeAllObjects];
            MCToolbarDebugLog("[MCToolbar] ClearItems itemOrder cleared");
            [m_delegate.itemImages removeAllObjects];
            MCToolbarDebugLog("[MCToolbar] ClearItems done");
        }
    }

    void SetDisplayMode(MCToolbarDisplayMode p_mode) override
    {
        if (m_toolbar)
            m_toolbar.displayMode = (NSToolbarDisplayMode)p_mode;
    }

    void SetVisible(bool p_visible) override
    {
        if (m_toolbar)
            m_toolbar.visible = p_visible ? YES : NO;
    }

    bool GetVisible() override
    {
        return m_toolbar ? (m_toolbar.visible == YES) : false;
    }

    // Called by the ObjC delegate when an item is clicked
    void OnItemClicked(MCNameRef p_name)
    {
        if (m_owner)
            m_owner->itemClicked(p_name);
    }

private:
    MCToolbar                      *m_owner;
    // Under ARC, ObjC pointers in C++ classes are __unsafe_unretained by
    // default.  Explicitly mark the objects we own as __strong so ARC
    // keeps them alive between Create() and Destroy().
    __strong NSToolbar             *m_toolbar;
    __strong MCNSToolbarDelegate   *m_delegate;
    // m_window is owned by the engine — do not retain it.
    __unsafe_unretained NSWindow   *m_window;

    static NSString *_stringRefToNSString(MCStringRef p_str)
    {
        if (!p_str || MCStringIsEmpty(p_str))
            return @"";
        return MCStringConvertToAutoreleasedNSString(p_str);
    }

    static NSString *_nameToNSString(MCNameRef p_name)
    {
        if (!p_name)
            return @"";
        return _stringRefToNSString(MCNameGetString(p_name));
    }
};

////////////////////////////////////////////////////////////////////////////////
// MCNSToolbarDelegate category — defined here so it can see the complete
// MCToolbarMacBackend class and call OnItemClicked().

@implementation MCNSToolbarDelegate (MCBackendActions)

- (void)toolbarItemClicked:(id)sender
{
    if (!_backend)
        return;

    NSString *ident = [(NSToolbarItem *)sender itemIdentifier];
    if (ident == nil)
        return;

    MCAutoStringRef t_str;
    /* UNCHECKED */ MCStringCreateWithCString([ident UTF8String], &t_str);
    MCNewAutoNameRef t_name;
    /* UNCHECKED */ MCNameCreate(*t_str, &t_name);
    _backend->OnItemClicked(*t_name);
}

@end

////////////////////////////////////////////////////////////////////////////////
// Factory

MCToolbarBackend *MCToolbarCreatePlatformBackend(MCToolbar *p_owner)
{
    return new MCToolbarMacBackend(p_owner);
}
