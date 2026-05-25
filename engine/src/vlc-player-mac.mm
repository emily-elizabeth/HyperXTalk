/* Copyright (C) 2024 HyperXTalk Contributors

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.  */

// vlc-player-mac.mm — macOS helper functions for MCVLCPlayer.
//
// Creates and manages the NSView that VLC renders into.

#import <Cocoa/Cocoa.h>

#include "globdefs.h"
#include "platform.h"
#include "graphics_util.h"

// ---------------------------------------------------------------------------
// MCVLCPlayerView — a plain layer-hosting NSView for VLC to render into.
// ---------------------------------------------------------------------------

@interface MCVLCPlayerView : NSView
{
    // Called (async, on the main queue) the first time the view's frame
    // transitions from zero to non-zero.  MCVLCPlayer::Start() arms this
    // callback when the native layer has not yet applied its deferred
    // geometry — the callback fires when that deferred geometry is finally
    // applied by MCNativeLayerMac::doSetGeometry() during the first paint.
    void  (*_frameReadyCb)(void *);
    void   *_frameReadyOpaque;
    BOOL    _frameCallbackFired;   // guard: fire at most once per view

    // Set by MCVLCDestroyNSView() to prevent the queued block from calling
    // back into a freed MCVLCPlayer.
    BOOL    _frameCancelled;

    // Deferred self-destruction state.  MCVLCDestroyNSView() calls
    // -markForDestruction which sets _destroyPending and tries to schedule
    // teardown immediately.  If VLC still has a rendering subview
    // (VLCVideoLayerView) at that point, teardown is deferred: the
    // -didRemoveSubview: override retries when VLC removes its subview.
    // This prevents use-after-free when VLC's background-thread cleanup
    // dispatches to the main queue after the MCVLCPlayer C++ destructor
    // has already returned.
    BOOL    _destroyPending;    // -markForDestruction called; waiting for VLC subviews
    BOOL    _destroyDispatched; // teardown dispatch_async already queued
}
- (void)setFrameReadyCallback:(void (*)(void *))callback opaque:(void *)opaque;
- (void)cancelFrameCallback;
// If the frame is already non-zero when the callback is armed, fire it on the
// next run-loop turn rather than waiting for a future setFrame: transition.
- (void)fireCallbackDeferredIfFrameReady;
// Called by MCVLCDestroyNSView().  Cancels the frame callback and schedules
// teardown for when all VLC rendering subviews have been removed.
- (void)markForDestruction;
@end

@implementation MCVLCPlayerView

- (void)setFrameReadyCallback:(void (*)(void *))callback opaque:(void *)opaque
{
    _frameReadyCb      = callback;
    _frameReadyOpaque  = opaque;
    _frameCallbackFired = NO;
    _frameCancelled    = NO;
}

- (void)cancelFrameCallback
{
    _frameCancelled = YES;
}

- (void)fireCallbackDeferredIfFrameReady
{
    NSRect t_frame = [self frame];
    if (NSWidth(t_frame) <= 0 || NSHeight(t_frame) <= 0)
        return;   // frame still zero — setFrame: will fire the callback later
    if (_frameReadyCb == nullptr || _frameCallbackFired)
        return;

    // Mark fired now so setFrame: won't double-fire.
    _frameCallbackFired = YES;

    void (*cb)(void *) = _frameReadyCb;
    void *op           = _frameReadyOpaque;
    [self retain];
    MCVLCPlayerView *view = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!view->_frameCancelled)
            cb(op);
        [view release];
    });
}

// ---------------------------------------------------------------------------
// Deferred self-destruction
// ---------------------------------------------------------------------------

// Called by MCVLCDestroyNSView() after the C++ MCVLCPlayer is torn down.
// Cancels the frame callback so it cannot fire into freed C++ memory, then
// attempts to schedule teardown.  If VLC has a rendering subview present
// (added by its vout module when play() was called), teardown is deferred
// until -didRemoveSubview: confirms VLC has finished its own cleanup.
- (void)markForDestruction
{
    fprintf(stderr, "[VLC-view] markForDestruction %p subviews=%lu superview=%p\n",
            (void *)self, (unsigned long)[[self subviews] count],
            (void *)[self superview]);
    fflush(stderr);
    [self cancelFrameCallback];
    if (_destroyPending || _destroyDispatched)
        return;

    // Remove from superview immediately and synchronously.
    //
    // AppKit registers a view in window-level structures (key view loop,
    // accessibility tree, display scheduling) when it is added as a subview.
    // If the parent view is dealloc'd before the child is formally removed,
    // those registrations become dangling.  When the child is later dealloc'd
    // and NSView tries to unregister from them, it dereferences freed memory
    // — SIGSEGV in [super dealloc].
    //
    // Calling removeFromSuperview here — while the C++ destructor is still
    // on the call stack and the parent is guaranteed alive — ensures the view
    // is cleanly unregistered from AppKit before anything is freed.
    //
    // Note: we do NOT release here.  MCVLCCreateNSView's alloc-reference
    // (retain count = 1 on entry, or 2 if still in superview) keeps the view
    // alive until -tryDestroy's dispatch_async teardown block fires.
    if ([self superview] != nil)
        [self removeFromSuperview];

    _destroyPending = YES;
    [self tryDestroy];
}

// Schedules the view's removal from its superview and final release, provided
// all VLC rendering subviews are gone.  Safe to call multiple times; only the
// first qualifying call dispatches the teardown block.
- (void)tryDestroy
{
    if (!_destroyPending || _destroyDispatched)
        return;

    NSUInteger t_count = [[self subviews] count];
    fprintf(stderr, "[VLC-view] tryDestroy %p subviews=%lu\n",
            (void *)self, (unsigned long)t_count);
    fflush(stderr);

    // If VLC still has a rendering subview (e.g. VLCVideoLayerView), wait.
    // -didRemoveSubview: will call us again when that subview is removed.
    if (t_count > 0)
        return;

    _destroyDispatched = YES;
    _destroyPending    = NO;

    fprintf(stderr, "[VLC-view] tryDestroy %p dispatching teardown\n",
            (void *)self);
    fflush(stderr);

    // Defer the final release by one main-queue turn.
    //
    // removeFromSuperview was already called synchronously in
    // -markForDestruction, so we only need to release the view here.
    // autorelease (rather than release) defers dealloc to the end of the
    // run-loop iteration, after any pending Core Animation transactions
    // involving the view's backing layer have committed — preventing a
    // SIGSEGV if CA tries to draw into an otherwise already-freed layer.
    //
    // Ownership: the alloc-reference from MCVLCCreateNSView (retain count = 1
    // after removeFromSuperview dropped the superview's reference) is balanced
    // by the autorelease here: block-copy auto-retain (+1) is cancelled by
    // block-dealloc auto-release (-1), leaving the autorelease as the sole
    // pending -1, which fires at pool-drain time → count = 0 → dealloc.
    dispatch_async(dispatch_get_main_queue(), ^{
        fprintf(stderr, "[VLC-view] teardown block firing %p\n", (void *)self);
        fflush(stderr);
        [self autorelease];
    });
}

// AppKit calls this whenever a subview is removed from this view.
// When VLC removes its rendering subview (VLCVideoLayerView) as part of
// its async vout teardown, we check whether it is now safe to proceed
// with our own teardown.
- (void)didRemoveSubview:(NSView *)subview
{
    [super didRemoveSubview:subview];
    fprintf(stderr, "[VLC-view] didRemoveSubview %p removed=%p pending=%d dispatched=%d remaining=%lu\n",
            (void *)self, (void *)subview,
            (int)_destroyPending, (int)_destroyDispatched,
            (unsigned long)[[self subviews] count]);
    fflush(stderr);
    if (_destroyPending && !_destroyDispatched)
        [self tryDestroy];
}

- (BOOL)isOpaque
{
    return YES;
}

// Do NOT override wantsLayer — VLC 3's macosx vout module uses OpenGL and
// requires a plain (non-layer-backed) NSView as its rendering target.
// A layer-backed view conflicts with VLC's GL context setup and produces
// a blank output.  Use drawRect: for the black fill instead.

- (void)drawRect:(NSRect)dirtyRect
{
    [[NSColor blackColor] set];
    NSRectFill(dirtyRect);
}

// Called by AppKit (and directly by MCNativeLayerMac::doSetGeometry) whenever
// the view's frame changes.  We use the first zero→non-zero transition to
// signal MCVLCPlayer::Start() that the native layer has applied its deferred
// geometry and VLC's vout can now be safely initialized.
//
// Background: MCNativeLayer::m_rect starts as {0,0,0,0} and
// m_defer_geometry_changes starts true, so doAttach() calls
// doSetGeometry({0,0,0,0}).  The actual player rect lives in m_deferred_rect
// and is applied in OnPaint() — which runs asynchronously.  If Start() is
// called in the same run-loop turn as attachplayer() (e.g. from an openStack
// handler), libvlc_media_player_play() fires while the view has a zero frame.
// VLC's macOS vout then creates a zero-size GL surface and silently fails to
// render video.  This override catches the moment the frame becomes valid so
// the deferred play can be triggered at exactly the right time.
- (void)setFrame:(NSRect)frameRect
{
    BOOL t_was_zero = (NSWidth([self frame]) == 0 || NSHeight([self frame]) == 0);
    [super setFrame: frameRect];

    BOOL t_now_nonzero = (NSWidth(frameRect) > 0 && NSHeight(frameRect) > 0);
    if (t_was_zero && t_now_nonzero && !_frameCallbackFired
        && _frameReadyCb != nullptr)
    {
        _frameCallbackFired = YES;

        // Retain self so the view remains valid until the block fires.
        // _frameCancelled is set by MCVLCDestroyNSView() if the player is
        // torn down before the block runs.
        void (*cb)(void *) = _frameReadyCb;
        void *op           = _frameReadyOpaque;
        [self retain];
        MCVLCPlayerView *view = self;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!view->_frameCancelled)
                cb(op);
            [view release];
        });
    }
}

- (void)dealloc
{
    fprintf(stderr,
            "[VLC-view] dealloc %p window=%p superview=%p subviews=%lu "
            "hidden=%d frame=(%.0f,%.0f,%.0f,%.0f)\n",
            (void *)self,
            (void *)[self window],
            (void *)[self superview],
            (unsigned long)[[self subviews] count],
            (int)[self isHidden],
            [self frame].origin.x,    [self frame].origin.y,
            [self frame].size.width,  [self frame].size.height);
    fflush(stderr);
    _frameReadyCb      = nullptr;
    _frameReadyOpaque  = nullptr;
    [super dealloc];
}

@end

// ---------------------------------------------------------------------------
// C helper API called from vlc-player.cpp
// ---------------------------------------------------------------------------

extern "C"
{

void *MCVLCCreateNSView(void)
{
    MCVLCPlayerView *t_view =
        [[MCVLCPlayerView alloc] initWithFrame: NSZeroRect];
    if (t_view == nil)
        return nullptr;

    return (void *)t_view;
}

void MCVLCDestroyNSView(void *p_view)
{
    if (p_view == nullptr)
        return;

    MCVLCPlayerView *t_view = (MCVLCPlayerView *)p_view;

    // -markForDestruction cancels the frame-ready callback (so it cannot
    // fire into the freed MCVLCPlayer C++ object) and schedules teardown.
    //
    // If VLC already removed its rendering subview (VLCVideoLayerView), the
    // teardown dispatch_async is queued immediately.  If the subview is still
    // present — because VLC's vout cleanup dispatched asynchronously and may
    // not yet have fired — teardown is deferred: the -didRemoveSubview: override
    // retries when VLC eventually removes the subview.  This avoids the
    // use-after-free / malloc-free-list corruption that results from our teardown
    // block racing ahead of VLC's background-thread cleanup blocks.
    //
    // Ownership: the alloc-reference from MCVLCCreateNSView (retain count = 1
    // on entry) is passed to the view itself; it will be balanced by the
    // autorelease inside the dispatch_async block that -tryDestroy queues.
    [t_view markForDestruction];
}

void MCVLCReparentNSView(void *p_view, void *p_parent_view)
{
    if (p_view == nullptr)
        return;

    MCVLCPlayerView *t_view   = (MCVLCPlayerView *)p_view;
    NSView          *t_parent = (NSView *)p_parent_view;

    // Remove from any existing superview first.
    if ([t_view superview] != nil && [t_view superview] != t_parent)
        [t_view removeFromSuperview];

    if (t_parent != nil && [t_view superview] == nil)
        [t_parent addSubview: t_view];
}

// Arm a one-shot callback that fires (asynchronously on the main queue) the
// first time the view's frame transitions from zero to non-zero.
//
// If the view already has a non-zero frame at the time this is called, the
// callback is dispatched immediately on the next main-queue turn instead of
// waiting for a future setFrame: call.  This handles the reopen case where
// MCNativeLayerMac has already applied its deferred geometry before Start()
// runs, so the setFrame: zero→non-zero transition already happened.
//
// Deferring play to the next run-loop turn (even when the frame is already
// valid) is critical: it lets AppKit finish all pending layout and compositing
// callbacks for the current turn before VLC initialises its vout surface.
void MCVLCSetFrameReadyCallback(void *p_view,
                                void (*p_callback)(void *),
                                void  *p_opaque)
{
    if (p_view == nullptr)
        return;

    MCVLCPlayerView *t_view = (MCVLCPlayerView *)p_view;
    [t_view setFrameReadyCallback: p_callback opaque: p_opaque];

    // If the frame is already non-zero, fire the callback on the next
    // run-loop turn (mirrors what the setFrame: override would do for the
    // zero→non-zero transition).
    [t_view fireCallbackDeferredIfFrameReady];
}

// Returns true when the view has a non-zero frame.
bool MCVLCViewHasNonZeroFrame(void *p_view)
{
    if (p_view == nullptr)
        return false;

    MCVLCPlayerView *t_view = (MCVLCPlayerView *)p_view;
    NSRect t_frame = [t_view frame];
    return NSWidth(t_frame) > 0 && NSHeight(t_frame) > 0;
}

// Returns true when the view is part of a live window hierarchy.
bool MCVLCViewHasWindow(void *p_view)
{
    if (p_view == nullptr)
        return false;

    MCVLCPlayerView *t_view = (MCVLCPlayerView *)p_view;
    return [t_view window] != nil;
}

void MCVLCSyncNSView(void *p_view, MCRectangle p_rect, bool p_visible)
{
    if (p_view == nullptr)
        return;

    MCVLCPlayerView *t_view = (MCVLCPlayerView *)p_view;

    if (!p_visible)
    {
        [t_view setHidden: YES];
        return;
    }

    if (p_rect.width == 0 && p_rect.height == 0)
    {
        // Rect not yet set (native layer hasn't applied deferred geometry yet).
        // Don't position the view, but don't hide it either — the native layer
        // will call setFrame: with the real rect shortly, and we must be visible
        // when that happens so VLC's vout renders correctly.
        [t_view setHidden: NO];
        return;
    }

    NSView *t_parent = [t_view superview];
    if (t_parent == nil)
    {
        // Not yet embedded — nothing to position.
        [t_view setHidden: NO];
        return;
    }

    // Convert from HyperXTalk (top-left origin) to Cocoa (bottom-left origin).
    NSRect t_parent_bounds = [t_parent bounds];
    NSRect t_frame = NSMakeRect(
        p_rect.x,
        (CGFloat)t_parent_bounds.size.height - p_rect.y - p_rect.height,
        p_rect.width,
        p_rect.height);

    [t_view setFrame: t_frame];
    [t_view setHidden: NO];
}

// Dump the full subview tree of MCVLCPlayerView to stderr.
// Called 600 ms after play() to confirm VLC added its rendering subview.
void MCVLCDumpViewHierarchy(void *p_view)
{
    if (p_view == nullptr)
        return;

    MCVLCPlayerView *t_root = (MCVLCPlayerView *)p_view;
    [t_root retain];

    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)(600 * NSEC_PER_MSEC)),
        dispatch_get_main_queue(),
        ^{
            NSRect t_rf = [t_root frame];
            fprintf(stderr,
                    "[VLC-hier] MCVLCPlayerView %p frame=(%.0f,%.0f,%.0f,%.0f)"
                    " hidden=%d inWindow=%d\n",
                    (void *)t_root,
                    t_rf.origin.x, t_rf.origin.y,
                    t_rf.size.width, t_rf.size.height,
                    (int)[t_root isHidden],
                    (int)([t_root window] != nil));

            NSArray<NSView *> *t_subs = [t_root subviews];
            fprintf(stderr, "[VLC-hier]   subviews: %lu\n",
                    (unsigned long)[t_subs count]);
            for (NSView *sv in t_subs)
            {
                NSRect sf = [sv frame];
                fprintf(stderr,
                        "[VLC-hier]     %s %p"
                        " frame=(%.0f,%.0f,%.0f,%.0f) hidden=%d\n",
                        [[sv className] UTF8String], (void *)sv,
                        sf.origin.x, sf.origin.y,
                        sf.size.width, sf.size.height,
                        (int)[sv isHidden]);
            }

            // Also dump the superview chain so we can see if any ancestor is hidden.
            NSView *t_sv = [t_root superview];
            int t_depth = 0;
            while (t_sv != nil && t_depth < 6)
            {
                NSRect pf = [t_sv frame];
                fprintf(stderr,
                        "[VLC-hier] superview[%d] %s %p"
                        " frame=(%.0f,%.0f,%.0f,%.0f) hidden=%d\n",
                        t_depth,
                        [[t_sv className] UTF8String], (void *)t_sv,
                        pf.origin.x, pf.origin.y,
                        pf.size.width, pf.size.height,
                        (int)[t_sv isHidden]);
                t_sv = [t_sv superview];
                t_depth++;
            }

            [t_root release];
        });
}

} // extern "C"
