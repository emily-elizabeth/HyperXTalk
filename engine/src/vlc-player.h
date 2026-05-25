/* Copyright (C) 2024 HyperXTalk Contributors

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.  */

// vlc-player.h — MCVLCPlayer: libVLC-backed cross-platform media player.
//
// Implements MCPlatformPlayer using the libVLC 3 C API.  One libvlc_instance_t
// is shared across all players (initialised lazily on first use).  Each player
// owns a libvlc_media_player_t.
//
// Platform-specific window embedding:
//   macOS   — libvlc_media_player_set_nsobject(mp, NSView*)
//   Windows — libvlc_media_player_set_hwnd(mp, HWND)
//   Linux   — libvlc_media_player_set_xwindow(mp, XID)
//
// VLC fires all events from its internal thread; they are marshalled back to
// the HyperXTalk main thread via MCNotifyPush before touching engine state.

#pragma once

#include "platform.h"
#include "platform-internal.h"

// ---------------------------------------------------------------------------

class MCVLCPlayer : public MCPlatformPlayer
{
public:
    MCVLCPlayer();
    virtual ~MCVLCPlayer();

    // ---- MCPlatformPlayer interface ----------------------------------------

    virtual bool GetNativeView(void *&r_view) override;
    virtual bool SetNativeParentView(void *p_parent_view) override;

    virtual bool IsPlaying() override;
    virtual void Start(double rate) override;
    virtual void Stop() override;
    virtual void Step(int amount) override;

    virtual bool LockBitmap(const MCGIntegerSize &p_size,
                            MCImageBitmap *&r_bitmap) override;
    virtual void UnlockBitmap(MCImageBitmap *bitmap) override;

    virtual void SetProperty(MCPlatformPlayerProperty property,
                             MCPlatformPropertyType type,
                             void *value) override;
    virtual void GetProperty(MCPlatformPlayerProperty property,
                             MCPlatformPropertyType type,
                             void *value) override;

    virtual void CountTracks(uindex_t &r_count) override;
    virtual bool FindTrackWithId(uint32_t id, uindex_t &r_index) override;
    virtual void SetTrackProperty(uindex_t index,
                                  MCPlatformPlayerTrackProperty property,
                                  MCPlatformPropertyType type,
                                  void *value) override;
    virtual void GetTrackProperty(uindex_t index,
                                  MCPlatformPlayerTrackProperty property,
                                  MCPlatformPropertyType type,
                                  void *value) override;

protected:
    virtual void Realize() override;
    virtual void Unrealize() override;

private:
    // -- Lifecycle helpers ---------------------------------------------------
    void Load(MCStringRef p_filename, bool p_is_url);
    void Unload();
    void Synchronize();              // apply m_rect / m_visible to native view
    void AttachNativeView();         // tell VLC about the native drawable
    void SetupOffscreenCallbacks();  // wire up video memory callbacks

    // -- VLC event callbacks (called on VLC internal thread) -----------------
    static void OnVLCEvent(const struct libvlc_event_t *p_event, void *p_opaque);

#if defined(TARGET_PLATFORM_MACOS_X)
    // Called (on main thread, async) by MCVLCPlayerView when its frame first
    // becomes non-zero.  Starts the deferred libvlc_media_player_play() so
    // VLC's vout initializes with a properly-sized NSView.
    static void OnFrameReady(void *p_opaque);
#endif

#if defined(TARGET_PLATFORM_WINDOWS)
    // WndProc for the VLC render-surface HWND.  Handles WM_SIZE so that
    // VLC's D3D vout is reattached when MCNativeLayerWin32 resizes the
    // window after initial creation.
    static LRESULT CALLBACK s_vlc_wnd_proc(HWND hwnd, UINT msg,
                                            WPARAM wp, LPARAM lp);
#endif

    // -- Main-thread notification dispatchers --------------------------------
    static void DoTimeChanged(void *p_opaque);
    static void DoFinished(void *p_opaque);
    static void DoPlaying(void *p_opaque);

    // -- Offscreen rendering callbacks (for LockBitmap) ----------------------
    static void *OnVideoLock(void *p_opaque, void **pp_planes);
    static void  OnVideoUnlock(void *p_opaque, void *p_picture,
                               void *const *pp_planes);
    static void  OnVideoDisplay(void *p_opaque, void *p_picture);
    static unsigned OnVideoFormat(void **pp_opaque, char *p_chroma,
                                  unsigned *p_width, unsigned *p_height,
                                  unsigned *p_pitches,
                                  unsigned *p_lines);
    static void  OnVideoCleanup(void *p_opaque);

    // -- Shared VLC instance -------------------------------------------------
    static struct libvlc_instance_t *s_vlc_instance;
    static unsigned                  s_vlc_refcount;

    static bool EnsureVLCInstance();
    static void ReleaseVLCInstance();

    // -- Per-player VLC state ------------------------------------------------
    struct libvlc_media_player_t *m_player;
    struct libvlc_media_t        *m_media;

    // -- Platform-specific native view / window handle -----------------------
    void *m_view;       // NSView* on Mac, HWND on Win, X11 Window on Linux
#if defined(TARGET_PLATFORM_LINUX)
    unsigned long m_colormap;
#endif
#if defined(TARGET_PLATFORM_MACOS_X)
    // Set by Realize(), cleared by Unrealize() and Synchronize() once both
    // conditions (in-window, non-zero frame) are met.
    bool m_needs_vout_reattach;

    // Deferred play state.  Set by Start() when the native layer's initial
    // frame is still {0,0,0,0} (deferred geometry).  OnFrameReady() starts
    // play when the first non-zero frame is applied by MCNativeLayerMac.
    bool   m_play_pending_mac;
    double m_pending_rate_mac;
#endif

    // -- Playback state ------------------------------------------------------
    MCRectangle m_rect;
    bool        m_visible;
    bool        m_offscreen;
    bool        m_looping;
    bool        m_play_selection_only;
    double      m_rate;
    uint16_t    m_volume;       // 0-100

    MCPlatformPlayerDuration m_selection_start;
    MCPlatformPlayerDuration m_selection_finish;

    bool m_playing;
    bool m_finished;
    bool m_has_invalid_filename;
#if defined(TARGET_PLATFORM_WINDOWS)
    // Set by Start() when the HWND is not yet sized; cleared when play
    // actually begins (from WM_SIZE or the fallback timer).
    bool m_play_pending;

    // Set in WM_DESTROY when the destruction was NOT initiated by us
    // (i.e. cascade-destroyed by MCNativeLayerWin32 tearing down its
    // viewport container).  GetNativeView() / Start() check this and
    // call RecreateNativeView() before returning / playing.
    bool  m_view_dead;

    // Set to true just before we call DestroyWindow ourselves so the
    // WM_DESTROY handler can distinguish our intentional teardown from
    // an external cascade destruction.
    bool  m_destroying;

    // The parent HWND passed to the most recent SetNativeParentView()
    // call.  Used by RecreateNativeView() to re-parent the new child.
    void *m_parent_view;

    // Creates a new render-surface HWND (same class / style as the
    // original) when the old one was cascade-destroyed externally.
    void RecreateNativeView();
#endif

    MCPlatformPlayerDuration m_buffered_time;

    // -- Markers -------------------------------------------------------------
    MCPlatformPlayerDuration *m_markers;
    uindex_t                  m_marker_count;
    MCPlatformPlayerDuration  m_last_marker;

    // -- Offscreen (LockBitmap) rendering ------------------------------------
    MCImageBitmap *m_offscreen_bitmap;
    unsigned       m_offscreen_width;
    unsigned       m_offscreen_height;
    void          *m_offscreen_planes[1];  // single-plane buffer
};

// ---------------------------------------------------------------------------
// Factory (called from platform-player.cpp)
MCVLCPlayer *MCVLCPlayerCreate();
