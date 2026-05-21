/* Copyright (C) 2024 HyperXTalk Contributors

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.  */

// vlc-player.cpp — MCVLCPlayer implementation.
//
// Cross-platform libVLC 3 media player backend for HyperXTalk.
//
// Platform-specific helper functions (NSView creation etc.) live in
// vlc-player-mac.mm (macOS) or are compiled inline below.

#include "prefix.h"

#include "globdefs.h"
#include "imagebitmap.h"
#include "region.h"
#include "notify.h"

#include "platform.h"
#include "platform-internal.h"

#include "graphics_util.h"

#include "vlc-player.h"

// Pull in the VLC headers as plain C.
#include <vlc/vlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_player.h>
#include <vlc/libvlc_events.h>

#if defined(TARGET_PLATFORM_MACOS_X)
#include <dlfcn.h>    // dladdr / Dl_info — used in EnsureVLCInstance to locate the bundle
#include <unistd.h>   // access() — used to probe candidate plugin directories
#include <stdlib.h>   // setenv() — used to set VLC_PLUGIN_PATH before libvlc_new
#elif defined(TARGET_PLATFORM_LINUX)
#include <unistd.h>   // readlink, access
#include <stdlib.h>   // setenv
#include <limits.h>   // PATH_MAX
// gdk/gdkx.h can't be included directly — its X11 types clash with the
// engine's GdkWindow* typedefs in sysdefs.h.  Wrap it in a namespace like
// lnxprefix.h does, so we can call x11::gdk_x11_drawable_get_xid().
namespace x11 {
#include <gdk/gdkx.h>
}
#endif

// ---------------------------------------------------------------------------
// Temporary diagnostic logger — writes to %TEMP%\vlc-debug.log on Windows,
// stderr on other platforms.  Remove once rendering is confirmed working.
// ---------------------------------------------------------------------------
#if defined(TARGET_PLATFORM_WINDOWS)
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>   // tolower — used in EnumProcessModules path comparison
#include <io.h>      // _open_osfhandle, _dup2, _close
#include <fcntl.h>   // _O_WRONLY, _O_TEXT
#include <shlobj.h>  // SHGetFolderPathA, CSIDL_LOCAL_APPDATA, SHGFP_TYPE_CURRENT
static void vlc_log(const char *fmt, ...)
{
    static FILE *s_log = nullptr;
    if (s_log == nullptr)
    {
        char t_path[MAX_PATH];
        if (GetTempPathA(MAX_PATH, t_path))
            strcat_s(t_path, "vlc-debug.log");
        else
            strcpy_s(t_path, "C:\\vlc-debug.log");
        s_log = fopen(t_path, "w");
        if (s_log)
            fprintf(s_log, "=== VLC debug log ===\n");
    }
    if (s_log)
    {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(s_log, fmt, ap);
        va_end(ap);
        fflush(s_log);
    }
}

// Link the version-info helper library (MSVC pragma, harmless if already linked).
#pragma comment(lib, "version.lib")
#pragma comment(lib, "psapi.lib")
#include <psapi.h>

// Log the 4-part file version of a DLL on disk.
// Kept in its own function (no C++ objects with dtors) so that SEH wrappers
// in callers are not affected by it.
static void vlc_log_dll_version(const char *p_path)
{
    DWORD t_dummy = 0;
    DWORD t_sz = GetFileVersionInfoSizeA(p_path, &t_dummy);
    if (t_sz == 0)
    {
        vlc_log("[VLC] ver(%s): n/a (err=%lu)\n",
                p_path, (unsigned long)GetLastError());
        return;
    }
    void *t_buf = malloc(t_sz);
    if (!t_buf)
        return;
    if (GetFileVersionInfoA(p_path, 0, t_sz, t_buf))
    {
        VS_FIXEDFILEINFO *t_ffi = nullptr;
        UINT t_len = 0;
        if (VerQueryValueA(t_buf, "\\", (LPVOID *)&t_ffi, &t_len) && t_ffi)
            vlc_log("[VLC] ver(%s): %u.%u.%u.%u\n", p_path,
                    (unsigned)HIWORD(t_ffi->dwFileVersionMS),
                    (unsigned)LOWORD(t_ffi->dwFileVersionMS),
                    (unsigned)HIWORD(t_ffi->dwFileVersionLS),
                    (unsigned)LOWORD(t_ffi->dwFileVersionLS));
    }
    free(t_buf);
}

// -----------------------------------------------------------------------
// Vectored Exception Handler — installed around all libvlc_new attempts.
//
// A VEH is invoked BEFORE any frame-based __try/__except handler in the
// call stack, including VLC's own internal SEH wrappers.  This means we
// see every real fault (access violation, illegal instruction, etc.) at
// the exact instruction that caused it, even if VLC catches and swallows
// the exception a few frames up and returns null silently.
//
// The handler logs the faulting RIP, the bad address (for AV), and which
// loaded module owns the faulting instruction — then returns
// EXCEPTION_CONTINUE_SEARCH so VLC's normal handler still runs.
// -----------------------------------------------------------------------
static LONG WINAPI vlc_crash_veh(EXCEPTION_POINTERS *ep)
{
    DWORD t_code = ep->ExceptionRecord->ExceptionCode;

    // Skip C++ exception dispatch (0xE06D7363) and other user-mode
    // exceptions used for normal flow control; log only real faults.
    if (t_code != EXCEPTION_ACCESS_VIOLATION   &&
        t_code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        t_code != EXCEPTION_IN_PAGE_ERROR       &&
        t_code != EXCEPTION_STACK_OVERFLOW      &&
        t_code != EXCEPTION_PRIV_INSTRUCTION)
        return EXCEPTION_CONTINUE_SEARCH;

    ULONG_PTR t_rip = (ULONG_PTR)ep->ContextRecord->Rip;
    vlc_log("[VLC] VEH fault 0x%08lX at RIP=0x%016llX\n",
            (unsigned long)t_code,
            (unsigned long long)t_rip);

    if (t_code == EXCEPTION_ACCESS_VIOLATION)
    {
        ULONG_PTR t_addr =
            (ULONG_PTR)ep->ExceptionRecord->ExceptionInformation[1];
        vlc_log("[VLC] VEH: %s address=0x%016llX\n",
                ep->ExceptionRecord->ExceptionInformation[0]
                    ? "WRITE to" : "READ from",
                (unsigned long long)t_addr);
    }

    // Identify which loaded module contains the faulting instruction.
    HMODULE t_fault_mod = nullptr;
    if (GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS      |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)t_rip, &t_fault_mod) && t_fault_mod)
    {
        char t_fault_path[MAX_PATH] = {};
        GetModuleFileNameA(t_fault_mod, t_fault_path, MAX_PATH);
        vlc_log("[VLC] VEH: module='%s' base=0x%016llX offset=+0x%llX\n",
                t_fault_path,
                (unsigned long long)(ULONG_PTR)t_fault_mod,
                (unsigned long long)(t_rip - (ULONG_PTR)t_fault_mod));
    }
    else
    {
        vlc_log("[VLC] VEH: faulting instruction not in any loaded module\n");
    }

    return EXCEPTION_CONTINUE_SEARCH; // let VLC's own handler deal with it
}

// -----------------------------------------------------------------------
// vlc_log_plugin_symbol — enumerate the export table of a VLC plugin DLL
// (loaded with DONT_RESOLVE_DLL_REFERENCES so its imports are never
// resolved) and log the "vlc_entry__<HASH>" function name.
//
// Every VLC plugin exports exactly ONE function whose name starts with
// "vlc_entry__".  The suffix after the double-underscore is the
// MODULE_SYMBOL: a build-time hash that libvlccore.dll uses as a
// compatibility stamp when loading plugins.  If a plugin was compiled
// against a different VLC build than the running libvlccore.dll — even
// at the same version number — its stamp will differ, and libvlccore will
// silently reject it with no error message.  Logging the stamp from both
// the bundled plugins and the system plugins reveals whether this is the
// root cause of "no logger module found".
// -----------------------------------------------------------------------
static void vlc_log_plugin_symbol(const char *p_path)
{
    HMODULE t_h = LoadLibraryExA(p_path, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!t_h)
    {
        vlc_log("[VLC] sym-check(%s): load err=%lu\n",
                p_path, (unsigned long)GetLastError());
        return;
    }
    bool t_found = false;
    __try
    {
        ULONG_PTR t_base = (ULONG_PTR)t_h;
        IMAGE_DOS_HEADER  *t_dos = (IMAGE_DOS_HEADER *)t_base;
        if (t_dos->e_magic != IMAGE_DOS_SIGNATURE) __leave;
        IMAGE_NT_HEADERS64 *t_nt =
            (IMAGE_NT_HEADERS64 *)(t_base + t_dos->e_lfanew);
        if (t_nt->Signature != IMAGE_NT_SIGNATURE) __leave;
        IMAGE_DATA_DIRECTORY *t_dd =
            &t_nt->OptionalHeader
                  .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!t_dd->VirtualAddress || !t_dd->Size) __leave;
        IMAGE_EXPORT_DIRECTORY *t_exp =
            (IMAGE_EXPORT_DIRECTORY *)(t_base + t_dd->VirtualAddress);
        DWORD *t_names = (DWORD *)(t_base + t_exp->AddressOfNames);
        for (DWORD t_i = 0; t_i < t_exp->NumberOfNames; t_i++)
        {
            const char *t_nm = (const char *)(t_base + t_names[t_i]);
            if (strncmp(t_nm, "vlc_entry__", 11) == 0)
            {
                vlc_log("[VLC] sym-check(%s): '%s'\n", p_path, t_nm);
                t_found = true;
                break; // one entry per plugin
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        vlc_log("[VLC] sym-check(%s): PE parse exception\n", p_path);
    }
    if (!t_found)
        vlc_log("[VLC] sym-check(%s): no vlc_entry__ export found\n",
                p_path);
    FreeLibrary(t_h);
}

// SEH-safe wrapper for libvlc_new.
// Must live in its own function (no C++ local objects with destructors)
// because MSVC forbids mixing __try/__except with such objects.
// Catching EXCEPTION_EXECUTE_HANDLER reveals silent crashes inside VLC's
// init path — the exception code tells us exactly what went wrong.
static libvlc_instance_t *vlc_new_safe(int argc, const char *const *argv)
{
    libvlc_instance_t *t_inst = nullptr;
    __try
    {
        t_inst = libvlc_new(argc, argv);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        vlc_log("[VLC] *** libvlc_new RAISED SEH EXCEPTION 0x%08lX ***\n",
                (unsigned long)GetExceptionCode());
    }
    return t_inst;
}

// Thread-based libvlc_new test.
// Some failures are thread-specific: COM apartment type set by the host app
// on the main thread, TLS slot exhaustion, or locks already held.
// Running on a fresh thread rules all of these in or out in one shot.
struct SVLCNewThreadArgs
{
    libvlc_instance_t *result;
    DWORD              exception_code;
};

static DWORD WINAPI vlc_new_thread_proc(LPVOID lp)
{
    SVLCNewThreadArgs *t = (SVLCNewThreadArgs *)lp;
    t->result         = nullptr;
    t->exception_code = 0;
    __try { t->result = libvlc_new(0, nullptr); }
    __except (EXCEPTION_EXECUTE_HANDLER)
    { t->exception_code = GetExceptionCode(); }
    return 0;
}
#else
#include <stdio.h>
#include <stdarg.h>
static void vlc_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
}

#endif

// ---------------------------------------------------------------------------
// Platform-specific helpers declared here, defined in vlc-player-mac.mm (Mac)
// or inline below (Win/Linux).
// ---------------------------------------------------------------------------

#if defined(TARGET_PLATFORM_MACOS_X)
// Implemented in vlc-player-mac.mm (compiled as Objective-C++, hence extern "C")
extern "C" void *MCVLCCreateNSView(void);
extern "C" void  MCVLCDestroyNSView(void *p_view);
extern "C" void  MCVLCReparentNSView(void *p_view, void *p_parent_view);
extern "C" void  MCVLCSyncNSView(void *p_view,
                                  MCRectangle p_rect, bool p_visible);
extern "C" void  MCVLCSetFrameReadyCallback(void *p_view,
                                             void (*p_callback)(void *),
                                             void  *p_opaque);
extern "C" bool  MCVLCViewHasNonZeroFrame(void *p_view);
extern "C" bool  MCVLCViewHasWindow(void *p_view);
#endif

// ---------------------------------------------------------------------------
// VLC internal log relay (macOS only — helps diagnose vout failures)
// ---------------------------------------------------------------------------
#if defined(TARGET_PLATFORM_MACOS_X)
static void vlc_internal_log_cb(void * /*data*/, int level,
                                 const libvlc_log_t * /*ctx*/,
                                 const char *fmt, va_list args)
{
    // Relay warnings and errors to our own stderr log so we can see VLC's
    // own vout/audio error messages without full verbose spam.
    if (level < LIBVLC_WARNING)
        return;
    char t_buf[512];
    vsnprintf(t_buf, sizeof(t_buf), fmt, args);
    // Strip trailing newline for consistency with our vlc_log() style.
    size_t t_len = strlen(t_buf);
    if (t_len > 0 && t_buf[t_len - 1] == '\n')
        t_buf[t_len - 1] = '\0';
    const char *t_lvl = (level == LIBVLC_ERROR) ? "ERR" :
                        (level == LIBVLC_WARNING) ? "WRN" : "INF";
    vlc_log("[VLC-internal %s] %s\n", t_lvl, t_buf);
}
#endif

// ---------------------------------------------------------------------------
// Shared VLC instance
// ---------------------------------------------------------------------------

libvlc_instance_t *MCVLCPlayer::s_vlc_instance = nullptr;
unsigned           MCVLCPlayer::s_vlc_refcount  = 0;

bool MCVLCPlayer::EnsureVLCInstance()
{
    if (s_vlc_instance != nullptr)
    {
        s_vlc_refcount++;
        return true;
    }

    // On macOS: locate VLC plugins.
    //
    // --plugin-path was removed in libVLC 4.  The correct mechanism is to set
    // the VLC_PLUGIN_PATH environment variable before calling libvlc_new.
    //
    // VLC.app structure:
    //   plugins/plugins/*.dylib  — the actual codec plugin dylibs (nested dir)
    //   plugins/*.jar etc        — support/data files at the top level
    //
    // The Makefile copies the entire VLC.app/plugins/ tree as vlc-plugins/, so
    // after bundling the dylibs land in vlc-plugins/plugins/.
    //
    // Priority:
    //   1. Bundled nested:  Contents/Resources/vlc-plugins/plugins  (dylibs here)
    //   2. Bundled flat:    Contents/Resources/vlc-plugins           (fallback)
    //   3. VLC.app nested:  /Applications/VLC.app/.../plugins/plugins (dylibs here)
    //   4. VLC.app flat:    /Applications/VLC.app/.../plugins         (fallback)
    //   5. Leave VLC_PLUGIN_PATH unset — libVLC will try its built-in defaults
#if defined(TARGET_PLATFORM_MACOS_X)
    // --- derive bundle Contents/ path from this binary's location ---
    Dl_info t_info;
    if (dladdr((void *)EnsureVLCInstance, &t_info) && t_info.dli_fname)
    {
        char t_contents[PATH_MAX];
        strlcpy(t_contents, t_info.dli_fname, sizeof(t_contents));
        char *t_macos = strstr(t_contents, "/MacOS/");
        if (t_macos)
        {
            *t_macos = '\0'; // t_contents now ends at ".../Contents"

            char t_probe[PATH_MAX];
            // The dylibs land in vlc-plugins/plugins/ after Makefile bundling.
            snprintf(t_probe, sizeof(t_probe),
                     "%s/Resources/vlc-plugins/plugins", t_contents);
            if (access(t_probe, F_OK) == 0)
            {
                setenv("VLC_PLUGIN_PATH", t_probe, 1);
            }
            else
            {
                // Flat layout fallback.
                snprintf(t_probe, sizeof(t_probe),
                         "%s/Resources/vlc-plugins", t_contents);
                if (access(t_probe, F_OK) == 0)
                    setenv("VLC_PLUGIN_PATH", t_probe, 1);
            }
        }
    }

    // --- fall back to VLC.app for development / non-packaged builds ---
    if (getenv("VLC_PLUGIN_PATH") == nullptr)
    {
        // VLC.app puts the actual dylibs in plugins/plugins/ (nested).
        const char *t_vlc_nested =
            "/Applications/VLC.app/Contents/MacOS/plugins/plugins";
        const char *t_vlc_flat =
            "/Applications/VLC.app/Contents/MacOS/plugins";

        if (access(t_vlc_nested, F_OK) == 0)
            setenv("VLC_PLUGIN_PATH", t_vlc_nested, 1);
        else if (access(t_vlc_flat, F_OK) == 0)
            setenv("VLC_PLUGIN_PATH", t_vlc_flat, 1);
    }

    // Guard: with -weak_library the symbol address is NULL if VLC.app is absent
    // and the standalone did not bundle the dylibs.  Return false gracefully so
    // player controls simply don't play rather than crashing.
    if (&libvlc_new == nullptr)
        return false;

    // Note: --quiet is intentionally omitted so libvlc_log_set() can relay
    // vout error/warning messages via vlc_internal_log_cb() above.
    const char *t_args_mac[] = {
        "--no-osd",
        "--no-stats",
    };
    s_vlc_instance = libvlc_new(2, t_args_mac);
    if (s_vlc_instance != nullptr)
        libvlc_log_set(s_vlc_instance, vlc_internal_log_cb, nullptr);
#elif defined(TARGET_PLATFORM_WINDOWS)
    static char s_plugin_path_arg[MAX_PATH + 32];
    s_plugin_path_arg[0] = '\0';

    // Derive exe directory and probe for bundled plugins.
    //   1. <exe>\vlc-plugins\plugins  (nested layout, matches VLC.app structure)
    //   2. <exe>\vlc-plugins          (flat layout)
    //   3. C:\Program Files\VideoLAN\VLC\plugins  (system VLC installation)
    char t_exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, t_exe_path, MAX_PATH))
    {
        char *t_sep = strrchr(t_exe_path, '\\');
        if (t_sep != nullptr)
            *t_sep = '\0';

        vlc_log("[VLC] exe dir: %s\n", t_exe_path);

        char t_probe[MAX_PATH];
        snprintf(t_probe, sizeof(t_probe),
                 "%s\\vlc-plugins\\plugins", t_exe_path);
        if (GetFileAttributesA(t_probe) != INVALID_FILE_ATTRIBUTES)
        {
            snprintf(s_plugin_path_arg, sizeof(s_plugin_path_arg),
                     "--plugin-path=%s", t_probe);
        }
        else
        {
            snprintf(t_probe, sizeof(t_probe),
                     "%s\\vlc-plugins", t_exe_path);
            if (GetFileAttributesA(t_probe) != INVALID_FILE_ATTRIBUTES)
                snprintf(s_plugin_path_arg, sizeof(s_plugin_path_arg),
                         "--plugin-path=%s", t_probe);
        }
    }

    // Fall back to system VLC installation.
    if (s_plugin_path_arg[0] == '\0')
    {
        const char *t_vlc_plugins =
            "C:\\Program Files\\VideoLAN\\VLC\\plugins";
        if (GetFileAttributesA(t_vlc_plugins) != INVALID_FILE_ATTRIBUTES)
            snprintf(s_plugin_path_arg, sizeof(s_plugin_path_arg),
                     "--plugin-path=%s", t_vlc_plugins);
    }

    vlc_log("[VLC] plugin arg: %s\n",
            s_plugin_path_arg[0] ? s_plugin_path_arg : "--no-plugins-scan");

    // Probe libvlccore.dll explicitly — error 126 means a dependency is missing,
    // error 193 means wrong bitness (32-bit DLL in a 64-bit process).
    // Also log the FULL PATH of the loaded module so we know which copy
    // (bundled vs system) Windows is actually using.
    {
        HMODULE t_core = LoadLibraryA("libvlccore.dll");
        if (t_core)
        {
            char t_core_path[MAX_PATH] = {};
            GetModuleFileNameA(t_core, t_core_path, MAX_PATH);
            vlc_log("[VLC] libvlccore.dll probe: OK (%p) path='%s'\n",
                    (void *)t_core, t_core_path);
            FreeLibrary(t_core);
        }
        else
        {
            DWORD t_err = GetLastError();
            vlc_log("[VLC] libvlccore.dll probe FAILED: error %lu\n",
                    (unsigned long)t_err);
        }
    }

    // Log the full path that the STATIC IMPORT of libvlc.dll resolved to.
    // This tells us which copy the process is actually bound to.
    {
        HMODULE t_vlcmod = GetModuleHandleA("libvlc.dll");
        if (t_vlcmod)
        {
            char t_vlc_path[MAX_PATH] = {};
            GetModuleFileNameA(t_vlcmod, t_vlc_path, MAX_PATH);
            vlc_log("[VLC] static libvlc.dll path: '%s'\n", t_vlc_path);
        }
        else
        {
            vlc_log("[VLC] static libvlc.dll: not found in process module list\n");
        }
    }

    // Compare file versions of the bundled and system copies of libvlccore.dll.
    // If they differ, the process has the bundled (old) copy loaded but we are
    // calling into the system (new) libvlc.dll — an ABI mismatch that would
    // cause silent crashes before VLC's error system ever initialises.
    {
        char t_bundled_core[MAX_PATH];
        snprintf(t_bundled_core, sizeof(t_bundled_core),
                 "%s\\libvlccore.dll", t_exe_path);
        vlc_log("[VLC] --- libvlccore.dll version comparison ---\n");
        vlc_log_dll_version(t_bundled_core);
        vlc_log_dll_version("C:\\Program Files\\VideoLAN\\VLC\\libvlccore.dll");
        vlc_log("[VLC] --- end version comparison ---\n");
    }

    // Log every .dll in VLC's install dir to see what's actually there.
    {
        static const char k_vlc[] = "C:\\Program Files\\VideoLAN\\VLC\\*.dll";
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(k_vlc, &fd);
        if (h != INVALID_HANDLE_VALUE)
        {
            do { vlc_log("[VLC] VLC dir dll: %s\n", fd.cFileName); }
            while (FindNextFileA(h, &fd));
            FindClose(h);
        }
        else
        {
            vlc_log("[VLC] VLC install dir not found or empty\n");
        }
    }

    // SetDllDirectoryA adds one extra search slot (between System32 and CWD)
    // for all subsequent DLL loads in this process.  Pointing it at the VLC
    // install directory lets libvlccore.dll find its MinGW runtime DLLs.
    static const char k_vlc_install[] = "C:\\Program Files\\VideoLAN\\VLC";
    if (GetFileAttributesA(k_vlc_install) != INVALID_FILE_ATTRIBUTES)
    {
        SetDllDirectoryA(k_vlc_install);
        vlc_log("[VLC] SetDllDirectoryA -> %s\n", k_vlc_install);
    }

    // --- Basic libvlc symbol sanity check ---
    // If libvlc_get_version() returns the wrong string or causes an exception,
    // the loaded libvlc.dll is fundamentally broken in this process context.
    {
        const char *t_vlc_ver = nullptr;
        __try { t_vlc_ver = libvlc_get_version(); }
        __except (EXCEPTION_EXECUTE_HANDLER)
        { vlc_log("[VLC] libvlc_get_version() EXCEPTION 0x%08lX\n",
                  (unsigned long)GetExceptionCode()); }
        vlc_log("[VLC] libvlc_get_version() -> %s\n",
                t_vlc_ver ? t_vlc_ver : "(null)");
    }

    // --- Count plugin DLLs in the bundled plugins directory ---
    // If the count is 0 the build step didn't copy the plugins and VLC
    // will fail to initialise regardless of the plugin-path argument.
    if (s_plugin_path_arg[0])
    {
        // s_plugin_path_arg starts with "--plugin-path="; strip the prefix.
        const char *t_pdir = s_plugin_path_arg + strlen("--plugin-path=");
        char t_pglob[MAX_PATH];
        snprintf(t_pglob, sizeof(t_pglob), "%s\\*.dll", t_pdir);
        WIN32_FIND_DATAA t_pfd;
        HANDLE t_ph = FindFirstFileA(t_pglob, &t_pfd);
        int t_pdll_count = 0;
        if (t_ph != INVALID_HANDLE_VALUE)
        {
            do { t_pdll_count++; } while (FindNextFileA(t_ph, &t_pfd));
            FindClose(t_ph);
        }
        vlc_log("[VLC] plugin dir '%s': %d DLL(s) found\n", t_pdir, t_pdll_count);
    }

    // VLC_PLUGIN_PATH — VLC 3.x deprecated --plugin-path in favour of this
    // environment variable, which is read during vlc_plugins_load() before
    // any command-line arguments are processed.  Without it, libvlccore.dll
    // auto-detects its plugin directory as <libvlccore.dll dir>\plugins, which
    // resolves to C:\Program Files\HyperXTalk\Plugins (HyperXTalk's own empty
    // Plugins folder) instead of our bundled vlc-plugins\plugins.  VLC finds no
    // modules there, fails to initialise, and returns null from libvlc_new.
    static char s_vlc_plugin_env[MAX_PATH];
    snprintf(s_vlc_plugin_env, sizeof(s_vlc_plugin_env),
             "%s\\vlc-plugins\\plugins", t_exe_path);
    SetEnvironmentVariableA("VLC_PLUGIN_PATH", s_vlc_plugin_env);
    vlc_log("[VLC] VLC_PLUGIN_PATH -> %s\n", s_vlc_plugin_env);

    // -----------------------------------------------------------------------
    // Probe: verify that libvlccore.dll's own getenv() sees the value we just
    // set.  C runtime getenv() implementations often maintain a private
    // _environ[] cache initialised at DLL-attach time; SetEnvironmentVariableA
    // updates the Win32 process block but NOT a stale CRT cache.  If the probe
    // returns null or a different path, we also call _putenv() through the same
    // CRT to force an update of that cache.
    //
    // We test the three CRT DLLs most likely to be used by libvlccore.dll:
    //   msvcrt.dll       — MinGW-compiled VLC typically links this
    //   ucrtbase.dll     — modern UCRT on Windows 10+
    //   libvlccore.dll   — some builds export getenv directly
    // -----------------------------------------------------------------------
    {
        typedef char *(__cdecl *pfn_getenv_t)(const char *);
        typedef int   (__cdecl *pfn_putenv_t)(const char *);

        struct { const char *name; pfn_getenv_t getenv_fn; pfn_putenv_t putenv_fn; }
        t_crts[4] = {};
        int t_ncrt = 0;

        const char *t_crt_names[] = {
            "libvlccore.dll", "msvcrt.dll", "ucrtbase.dll", nullptr
        };
        for (int t_ci = 0; t_crt_names[t_ci] && t_ncrt < 4; t_ci++)
        {
            HMODULE t_h = GetModuleHandleA(t_crt_names[t_ci]);
            if (!t_h) t_h = LoadLibraryA(t_crt_names[t_ci]);
            if (!t_h) continue;
            pfn_getenv_t t_ge =
                (pfn_getenv_t)GetProcAddress(t_h, "getenv");
            pfn_putenv_t t_pe =
                (pfn_putenv_t)GetProcAddress(t_h, "_putenv");
            if (t_ge)
            {
                t_crts[t_ncrt].name     = t_crt_names[t_ci];
                t_crts[t_ncrt].getenv_fn = t_ge;
                t_crts[t_ncrt].putenv_fn = t_pe;
                t_ncrt++;
            }
        }

        vlc_log("[VLC] --- getenv probe (%d CRTs found) ---\n", t_ncrt);
        for (int t_ci = 0; t_ci < t_ncrt; t_ci++)
        {
            const char *t_val =
                t_crts[t_ci].getenv_fn("VLC_PLUGIN_PATH");
            vlc_log("[VLC]   %s: getenv(VLC_PLUGIN_PATH) = %s\n",
                    t_crts[t_ci].name,
                    t_val ? t_val : "(null) <-- CACHE MISS!");

            if (!t_val && t_crts[t_ci].putenv_fn)
            {
                // CRT cache is stale — push the value into it.
                char t_kv[MAX_PATH + 32];
                snprintf(t_kv, sizeof(t_kv),
                         "VLC_PLUGIN_PATH=%s", s_vlc_plugin_env);
                int t_rc = t_crts[t_ci].putenv_fn(t_kv);
                vlc_log("[VLC]   %s: _putenv(%s) -> %d\n",
                        t_crts[t_ci].name, t_kv, t_rc);

                // Confirm it took.
                const char *t_after =
                    t_crts[t_ci].getenv_fn("VLC_PLUGIN_PATH");
                vlc_log("[VLC]   %s: re-check = %s\n",
                        t_crts[t_ci].name,
                        t_after ? t_after : "(still null!)");
            }
        }
        vlc_log("[VLC] --- end getenv probe ---\n");
    }

    // Set VLC_VERBOSE as an env var — VLC reads this before parsing argv,
    // so it is active even if libvlc_InternalInit fails before the arg parser runs.
    SetEnvironmentVariableA("VLC_VERBOSE", "2");

    // Redirect stderr at the Windows handle level so VLC's MinGW CRT output
    // is captured.  MinGW-compiled DLLs call WriteFile(GetStdHandle(
    // STD_ERROR_HANDLE), ...) rather than using MSVC's FILE* stderr, so
    // freopen() alone is not sufficient — we must replace the OS handle.
    {
        char t_stderr_path[MAX_PATH];
        if (GetTempPathA(MAX_PATH, t_stderr_path))
            strcat_s(t_stderr_path, "vlc-stderr.log");
        else
            strcpy_s(t_stderr_path, "C:\\vlc-stderr.log");

        HANDLE t_hf = CreateFileA(t_stderr_path,
                                   GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, NULL);
        if (t_hf != INVALID_HANDLE_VALUE)
        {
            // Replace both the Win32 STD_ERROR handle (used by MinGW's CRT)
            // and MSVC's FILE* stderr (used by our own code).
            SetStdHandle(STD_ERROR_HANDLE, t_hf);
            int t_fd = _open_osfhandle((intptr_t)t_hf, _O_WRONLY | _O_TEXT);
            if (t_fd >= 0)
            {
                _dup2(t_fd, _fileno(stderr));
                _close(t_fd);
            }
            vlc_log("[VLC] STD_ERROR_HANDLE redirected -> %s\n", t_stderr_path);
        }
        else
        {
            vlc_log("[VLC] stderr redirect FAILED (err=%lu)\n",
                    (unsigned long)GetLastError());
        }
    }

    // -----------------------------------------------------------------------
    // Diagnostic: enumerate every DLL currently loaded in this process.
    // AV/security agents, shell extensions, and app-compat shims commonly
    // inject their own DLLs into every GUI process.  Any such DLL that hooks
    // ntdll heap routines, VirtualAlloc, CriticalSection APIs, or CRT
    // allocators can silently break libvlc_InternalCreate before VLC's error
    // system is even initialised.  We log every non-Windows-directory module
    // so we can spot unexpected injections in the output.
    // -----------------------------------------------------------------------
    {
        HMODULE t_mods[1024] = {};
        DWORD   t_needed = 0;
        if (EnumProcessModules(GetCurrentProcess(),
                               t_mods, sizeof(t_mods), &t_needed))
        {
            DWORD t_mod_count = t_needed / sizeof(HMODULE);
            vlc_log("[VLC] --- loaded modules (%lu total) ---\n",
                    (unsigned long)t_mod_count);

            // Get the Windows directory (typically C:\Windows) in lowercase
            // so we can skip OS-provided DLLs.
            char t_windir[MAX_PATH] = {};
            GetWindowsDirectoryA(t_windir, MAX_PATH);
            for (char *p = t_windir; *p; p++)
                *p = (char)tolower((unsigned char)*p);

            for (DWORD t_mi = 0; t_mi < t_mod_count; t_mi++)
            {
                char t_mod_path[MAX_PATH] = {};
                if (!GetModuleFileNameExA(GetCurrentProcess(),
                                          t_mods[t_mi], t_mod_path, MAX_PATH))
                    continue;

                // Make a lowercase copy for comparison only.
                char t_mod_lower[MAX_PATH] = {};
                strncpy_s(t_mod_lower, t_mod_path, _TRUNCATE);
                for (char *p = t_mod_lower; *p; p++)
                    *p = (char)tolower((unsigned char)*p);

                // Skip modules inside the Windows directory.
                if (strstr(t_mod_lower, t_windir) != nullptr)
                    continue;

                vlc_log("[VLC]   non-win: %s\n", t_mod_path);
            }
            vlc_log("[VLC] --- end loaded modules ---\n");
        }
        else
        {
            vlc_log("[VLC] EnumProcessModules failed (err=%lu)\n",
                    (unsigned long)GetLastError());
        }
    }

    // -----------------------------------------------------------------------
    // Diagnostic 1: compare file sizes of the bundled vs system libvlccore.dll.
    // Both report version 3.0.23.0, but if they were built from different
    // commits or with different compile flags the file sizes will differ —
    // a reliable proxy for "different build, different MODULE_SYMBOL".
    // -----------------------------------------------------------------------
    {
        struct { const char *label; const char *path; } t_cores[] = {
            { "bundled", "C:\\Program Files\\HyperXTalk\\libvlccore.dll" },
            { "system",  "C:\\Program Files\\VideoLAN\\VLC\\libvlccore.dll" },
        };
        vlc_log("[VLC] --- libvlccore.dll file-size comparison ---\n");
        DWORD t_sz[2] = { 0, 0 };
        for (int t_ci = 0; t_ci < 2; t_ci++)
        {
            WIN32_FILE_ATTRIBUTE_DATA t_fa = {};
            if (GetFileAttributesExA(t_cores[t_ci].path,
                                     GetFileExInfoStandard, &t_fa))
            {
                t_sz[t_ci] = t_fa.nFileSizeLow;
                vlc_log("[VLC]   %s: %lu bytes\n",
                        t_cores[t_ci].label,
                        (unsigned long)t_fa.nFileSizeLow);
            }
            else
            {
                vlc_log("[VLC]   %s: not found\n", t_cores[t_ci].label);
            }
        }
        if (t_sz[0] && t_sz[1])
            vlc_log("[VLC]   => %s\n",
                    t_sz[0] == t_sz[1] ? "SAME SIZE (likely same build)"
                                       : "DIFFERENT SIZE (different builds => MODULE_SYMBOL mismatch!)");
        vlc_log("[VLC] --- end file-size comparison ---\n");
    }

    // -----------------------------------------------------------------------
    // Diagnostic 2: read the MODULE_SYMBOL build stamp from plugin DLLs.
    // The stamp is the suffix after "vlc_entry__" in the plugin's exports.
    // libvlccore.dll silently discards any plugin whose stamp doesn't match.
    // We check the same plugin from both the bundled and system directories
    // so we can tell whether the mismatch (if any) is between the two, or
    // whether both match libvlccore.dll (ruling out this cause entirely).
    // -----------------------------------------------------------------------
    {
        char t_bplugin[MAX_PATH], t_splugin[MAX_PATH];
        snprintf(t_bplugin, sizeof(t_bplugin),
                 "%s\\vlc-plugins\\plugins\\access\\libfilesystem_plugin.dll",
                 t_exe_path);
        const char *t_splugin_path =
            "C:\\Program Files\\VideoLAN\\VLC\\plugins\\access\\"
            "libfilesystem_plugin.dll";

        vlc_log("[VLC] --- MODULE_SYMBOL check ---\n");
        vlc_log_plugin_symbol(t_bplugin);     // bundled
        vlc_log_plugin_symbol(t_splugin_path); // system
        vlc_log("[VLC] --- end MODULE_SYMBOL check ---\n");
    }

    // -----------------------------------------------------------------------
    // Diagnostic 3: try to explicitly load the VLC logger plugin.
    // VLC's InternalInit returns VLC_ENOMOD (-4) when module_need("logger")
    // fails.  The logger is an external plugin (liblogger_plugin.dll).
    // We search for it under both the bundled and system plugin directories
    // and attempt LoadLibraryA — if this fails, the dependency chain is
    // broken; if it succeeds, the DLL can be loaded but VLC still rejects
    // it (MODULE_SYMBOL mismatch or wrong bitness).
    // -----------------------------------------------------------------------
    {
        char t_bdir[MAX_PATH];
        snprintf(t_bdir, sizeof(t_bdir),
                 "%s\\vlc-plugins\\plugins", t_exe_path);

        // Candidate sub-paths for the logger plugin across VLC builds.
        const char *t_logger_sub[] = {
            "logger\\liblogger_plugin.dll",
            "misc\\liblogger_plugin.dll",
            "liblogger_plugin.dll",
        };
        const char *t_sys_base = "C:\\Program Files\\VideoLAN\\VLC\\plugins";

        vlc_log("[VLC] --- logger plugin load tests ---\n");
        for (int t_li = 0;
             t_li < (int)(sizeof(t_logger_sub)/sizeof(t_logger_sub[0]));
             t_li++)
        {
            // Bundled
            char t_lpath[MAX_PATH];
            snprintf(t_lpath, sizeof(t_lpath), "%s\\%s",
                     t_bdir, t_logger_sub[t_li]);
            if (GetFileAttributesA(t_lpath) != INVALID_FILE_ATTRIBUTES)
            {
                HMODULE t_lm = LoadLibraryA(t_lpath);
                vlc_log("[VLC] bundled logger '%s': %s (err=%lu)\n",
                        t_lpath,
                        t_lm ? "LOADED OK" : "FAILED",
                        t_lm ? 0UL : (unsigned long)GetLastError());
                if (t_lm) { vlc_log_plugin_symbol(t_lpath); FreeLibrary(t_lm); }
            }

            // System
            snprintf(t_lpath, sizeof(t_lpath), "%s\\%s",
                     t_sys_base, t_logger_sub[t_li]);
            if (GetFileAttributesA(t_lpath) != INVALID_FILE_ATTRIBUTES)
            {
                HMODULE t_lm = LoadLibraryA(t_lpath);
                vlc_log("[VLC] system logger '%s': %s (err=%lu)\n",
                        t_lpath,
                        t_lm ? "LOADED OK" : "FAILED",
                        t_lm ? 0UL : (unsigned long)GetLastError());
                if (t_lm) { vlc_log_plugin_symbol(t_lpath); FreeLibrary(t_lm); }
            }
        }
        vlc_log("[VLC] --- end logger plugin load tests ---\n");
    }

    // Guard: with /DELAYLOAD:libvlc.dll the import thunk resolves on first
    // call.  If libvlc.dll is absent the thunk raises a structured exception
    // before vlc_new_safe's __try is in scope.  A GetProcAddress probe on the
    // already-loaded module handle detects absence without triggering SEH.
    {
        HMODULE t_hmod = LoadLibraryExA("libvlc.dll", NULL,
                                        LOAD_LIBRARY_AS_DATAFILE);
        if (t_hmod == NULL)
        {
            vlc_log("[VLC] libvlc.dll not found — VLC not bundled, skipping init\n");
            return false;
        }
        FreeLibrary(t_hmod);
    }

    // Install the VEH so we capture crash addresses from ALL libvlc_new
    // attempts below, even when VLC's own __try/__except swallows the fault.
    PVOID t_veh = AddVectoredExceptionHandler(1 /*first*/, vlc_crash_veh);
    vlc_log("[VLC] VEH installed: %p\n", t_veh);

    // Build a --cache-file path inside %LOCALAPPDATA%\<AppName>\Cache\ so VLC
    // can persist its plugin scan results across launches.  This avoids the
    // 5-10 second rescan that occurs with --no-plugins-cache, and keeps the
    // cache in a user-writable location rather than next to the executable
    // (which may be in the protected Program Files directory).
    // The folder name is derived from the executable name so each standalone
    // app gets its own cache and there is no dependency on HyperXTalk itself.
    static char s_cache_file_arg[MAX_PATH + 32];
    s_cache_file_arg[0] = '\0';
    {
        char t_local_appdata[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
                             t_local_appdata) == S_OK)
        {
            // Get the exe filename and strip the .exe extension.
            char t_exe_full[MAX_PATH];
            char t_app_name[MAX_PATH] = "HyperXTalkApp";
            if (GetModuleFileNameA(NULL, t_exe_full, MAX_PATH))
            {
                char *t_fname = strrchr(t_exe_full, '\\');
                t_fname = t_fname ? t_fname + 1 : t_exe_full;
                strncpy(t_app_name, t_fname, sizeof(t_app_name) - 1);
                char *t_dot = strrchr(t_app_name, '.');
                if (t_dot) *t_dot = '\0';
            }
            // Ensure the cache directory exists, creating parent folders as needed.
            char t_cache_dir[MAX_PATH];
            char t_app_dir[MAX_PATH];
            snprintf(t_app_dir,   sizeof(t_app_dir),   "%s\\%s",        t_local_appdata, t_app_name);
            snprintf(t_cache_dir, sizeof(t_cache_dir), "%s\\%s\\Cache", t_local_appdata, t_app_name);
            CreateDirectoryA(t_app_dir,   NULL); // no-op if already exists
            CreateDirectoryA(t_cache_dir, NULL); // no-op if already exists
            snprintf(s_cache_file_arg, sizeof(s_cache_file_arg),
                     "--cache-file=%s\\vlc-plugins.dat", t_cache_dir);
        }
    }
    vlc_log("[VLC] cache file arg: %s\n",
            s_cache_file_arg[0] ? s_cache_file_arg : "(none, falling back to --no-plugins-cache)");

    // --verbose=2 makes VLC write its init diagnostics to stderr (now a file).
    // vlc_new_safe wraps libvlc_new in an SEH handler so any internal crash
    // is caught and logged rather than silently returning null.
    const char *t_args_win[] = {
        "--verbose=2",
        "--no-osd",
        "--no-stats",
        s_cache_file_arg[0] ? s_cache_file_arg : "--no-plugins-cache",
        s_plugin_path_arg[0] ? s_plugin_path_arg : "--no-plugins-scan",
    };
    s_vlc_instance = vlc_new_safe(5, t_args_win);
    vlc_log("[VLC] libvlc_new (bundled) -> %p  errmsg: %s\n",
            (void *)s_vlc_instance,
            s_vlc_instance == nullptr ? (libvlc_errmsg() ? libvlc_errmsg() : "(null)") : "ok");

    // Attempt 2: --no-plugins-scan only.
    // Completely disables external plugin loading — VLC uses only the
    // modules compiled into libvlccore.dll.  If THIS succeeds, the failure
    // is in VLC's plugin scanner / plugin DLL loading, not in core init.
    // If this also fails, the failure is in VLC's core init path.
    if (s_vlc_instance == nullptr)
    {
        vlc_log("[VLC] retrying with --no-plugins-scan only\n");
        const char *t_args_noplug[] = { "--no-plugins-scan" };
        s_vlc_instance = vlc_new_safe(1, t_args_noplug);
        vlc_log("[VLC] libvlc_new(no-plugins-scan) -> %p  errmsg: %s\n",
                (void *)s_vlc_instance,
                s_vlc_instance == nullptr ? (libvlc_errmsg() ? libvlc_errmsg() : "(null)") : "ok");
    }

    // Attempt 3: manually load one plugin DLL to confirm plugins can load at
    // all in this process.  A failure here (err=126 etc.) would mean the
    // plugins themselves have missing dependencies in HyperXTalk's context.
    {
        char t_plugin_path[MAX_PATH];
        snprintf(t_plugin_path, sizeof(t_plugin_path),
                 "%s\\vlc-plugins\\plugins\\access\\libfilesystem_plugin.dll",
                 t_exe_path);
        HMODULE t_plugin = LoadLibraryA(t_plugin_path);
        if (t_plugin)
        {
            vlc_log("[VLC] plugin load test (%s): OK\n", t_plugin_path);
            FreeLibrary(t_plugin);
        }
        else
        {
            vlc_log("[VLC] plugin load test FAILED err=%lu path='%s'\n",
                    (unsigned long)GetLastError(), t_plugin_path);
        }
    }

    // If bundled plugins failed, fall back to the system VLC installation.
    // IMPORTANT: VLC 3.x ignores --plugin-path entirely; it reads the
    // VLC_PLUGIN_PATH environment variable in module_LoadPlugins() instead.
    // We must update the env var before calling libvlc_new so VLC actually
    // searches the system plugins directory this time.
    if (s_vlc_instance == nullptr)
    {
        static const char k_sys_plugins[] =
            "C:\\Program Files\\VideoLAN\\VLC\\plugins";
        if (GetFileAttributesA(k_sys_plugins) != INVALID_FILE_ATTRIBUTES)
        {
            // Override VLC_PLUGIN_PATH to the system installation.
            // This is the directory VLC.exe itself uses — guaranteed to
            // match libvlccore.dll's built-in plugin API checksum.
            SetEnvironmentVariableA("VLC_PLUGIN_PATH", k_sys_plugins);
            vlc_log("[VLC] VLC_PLUGIN_PATH (system) -> %s\n", k_sys_plugins);

            const char *t_args_sys[] = {
                "--no-osd",
                "--no-stats",
                s_cache_file_arg[0] ? s_cache_file_arg : "--no-plugins-cache",
            };
            s_vlc_instance = vlc_new_safe(3, t_args_sys);
            vlc_log("[VLC] libvlc_new (system plugins) -> %p  errmsg: %s\n",
                    (void *)s_vlc_instance,
                    s_vlc_instance == nullptr
                        ? (libvlc_errmsg() ? libvlc_errmsg() : "(null)")
                        : "ok");
        }
    }

    // -----------------------------------------------------------------------
    // Diagnostic: bypass the static import entirely and load libvlc.dll
    // directly from VLC's own install directory using
    // LOAD_WITH_ALTERED_SEARCH_PATH.  This makes Windows resolve libvlccore.dll
    // and any other dependencies starting from the DLL's own directory, exactly
    // as if VLC launched itself — ruling out a search-path mismatch as the cause
    // of libvlc_new returning null.
    //
    // If this succeeds while the static-import calls above fail, the production
    // fix is to delay-load libvlc.dll (/DELAYLOAD:libvlc.dll in the linker) and
    // pre-load it from VLC's install dir before any libvlc_* calls.
    // -----------------------------------------------------------------------
    if (s_vlc_instance == nullptr)
    {
        const char *k_sys_libvlc = "C:\\Program Files\\VideoLAN\\VLC\\libvlc.dll";
        HMODULE t_vlcdll = LoadLibraryExA(k_sys_libvlc, NULL,
                                          LOAD_WITH_ALTERED_SEARCH_PATH);
        vlc_log("[VLC] LoadLibraryExA(VLC\\libvlc.dll) -> %p  err=%lu\n",
                (void *)t_vlcdll, (unsigned long)GetLastError());
        if (t_vlcdll)
        {
            // Log which copy of libvlccore.dll this freshly-loaded libvlc.dll
            // ends up bound to.  If it gets HyperXTalk's bundled copy (because
            // it was already in the process) and that copy is a different version,
            // libvlc_new will crash before setting any error message.
            {
                HMODULE t_core2 = GetModuleHandleA("libvlccore.dll");
                char t_core2_path[MAX_PATH] = {};
                if (t_core2)
                    GetModuleFileNameA(t_core2, t_core2_path, MAX_PATH);
                vlc_log("[VLC] direct-load: libvlccore.dll in process = '%s'\n",
                        t_core2_path[0] ? t_core2_path : "(not found)");
            }

            typedef libvlc_instance_t *(*pfnNew)(int, const char *const *);
            pfnNew pfn_new = (pfnNew)GetProcAddress(t_vlcdll, "libvlc_new");
            vlc_log("[VLC] GetProcAddress(libvlc_new) -> %p\n", (void *)pfn_new);
            if (pfn_new)
            {
                // Try zero-args first — if VLC can find its own plugins via
                // its own directory, this should be sufficient.
                // Wrapped in SEH to catch silent crashes.
                libvlc_instance_t *t_probe = nullptr;
                __try
                {
                    t_probe = pfn_new(0, nullptr);
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    vlc_log("[VLC] direct-load libvlc_new SEH exception: 0x%08lX\n",
                            (unsigned long)GetExceptionCode());
                }
                vlc_log("[VLC] direct-load libvlc_new(0,null) -> %p\n",
                        (void *)t_probe);
                if (t_probe)
                {
                    // Success!  We cannot mix a dynamically-loaded instance
                    // with the statically-imported libvlc_* functions, so
                    // release this probe instance and log the result.  The
                    // real fix is to pre-load libvlc.dll from VLC's directory
                    // (see comment above) so the static import resolves
                    // correctly from that point on.
                    typedef void (*pfnRelease)(libvlc_instance_t *);
                    pfnRelease pfn_release =
                        (pfnRelease)GetProcAddress(t_vlcdll, "libvlc_release");
                    if (pfn_release)
                        pfn_release(t_probe);
                    vlc_log("[VLC] direct-load succeeded — static import is "
                            "picking up wrong DLL; pre-load fix required\n");
                }
                else
                {
                    // Even the directly-loaded DLL fails — something more
                    // fundamental is wrong (corrupt install, wrong bitness,
                    // missing plugin directory, etc.).
                    typedef const char *(*pfnErrmsg)(void);
                    pfnErrmsg pfn_errmsg =
                        (pfnErrmsg)GetProcAddress(t_vlcdll, "libvlc_errmsg");
                    const char *t_err = pfn_errmsg ? pfn_errmsg() : nullptr;
                    vlc_log("[VLC] direct-load libvlc_new failed: %s\n",
                            t_err ? t_err : "(null)");
                }
            }
            FreeLibrary(t_vlcdll);
        }
    }
    // -----------------------------------------------------------------------
    // End direct-load diagnostic
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    // Thread isolation test.
    // All previous attempts ran on the main thread.  If this succeeds, the
    // failure is thread-specific — most likely because HyperXTalk's UI
    // framework has initialised COM in STA mode on the main thread and VLC
    // needs a clean apartment, or because of a TLS / mutex conflict.
    // A freshly created thread has none of that state.
    //
    // If this succeeds we keep the instance — it is perfectly safe to create
    // a libvlc_instance_t on one thread and use it from another.
    // -----------------------------------------------------------------------
    if (s_vlc_instance == nullptr)
    {
        vlc_log("[VLC] trying libvlc_new(0,null) on a fresh thread...\n");
        SVLCNewThreadArgs t_targs = { nullptr, 0 };
        HANDLE t_hthread = CreateThread(nullptr, 0,
                                        vlc_new_thread_proc,
                                        &t_targs, 0, nullptr);
        if (t_hthread)
        {
            WaitForSingleObject(t_hthread, 10000); // 10-second timeout
            CloseHandle(t_hthread);
            vlc_log("[VLC] fresh-thread result=%p  exception=0x%08lX\n",
                    (void *)t_targs.result,
                    (unsigned long)t_targs.exception_code);
            if (t_targs.result)
            {
                s_vlc_instance = t_targs.result;
                vlc_log("[VLC] fresh-thread libvlc_new SUCCEEDED — "
                        "issue is main-thread-specific\n");
            }
        }
        else
        {
            vlc_log("[VLC] CreateThread failed (err=%lu)\n",
                    (unsigned long)GetLastError());
        }
    }

    // -----------------------------------------------------------------------
    // Diagnostic: call libvlc_InternalCreate directly from libvlccore.dll.
    //
    // libvlc_new() does exactly two things:
    //   1. libvlc_InternalCreate()  — malloc + zeroinit of the libvlc_int_t
    //                                 object; only touches VLC's own heap.
    //   2. libvlc_InternalInit()    — loads plugins, parses options, starts
    //                                 threads, initialises COM subsystems, etc.
    //
    // By calling InternalCreate directly we can split the failure:
    //   • InternalCreate returns null / crashes → allocator or CRT init broken
    //   • InternalCreate returns non-null      → object allocation is fine;
    //     failure is somewhere inside InternalInit (plugin loading, registry
    //     access, thread creation, COM, etc.)
    //
    // We intentionally leak the allocated object — there is no public
    // InternalDestroy, and freeing a half-initialised libvlc_int_t is unsafe.
    // This is a one-shot diagnostic executed only when all real inits failed.
    // -----------------------------------------------------------------------
    if (s_vlc_instance == nullptr)
    {
        HMODULE t_core_ic = GetModuleHandleA("libvlccore.dll");
        if (t_core_ic == nullptr)
            t_core_ic = LoadLibraryA("libvlccore.dll");

        if (t_core_ic)
        {
            typedef void *(*pfnInternalCreate)(void);
            typedef int   (*pfnInternalInit)(void *p_libvlc, int argc,
                                             const char *const *argv);

            pfnInternalCreate pfn_ic = (pfnInternalCreate)
                GetProcAddress(t_core_ic, "libvlc_InternalCreate");
            pfnInternalInit   pfn_ii = (pfnInternalInit)
                GetProcAddress(t_core_ic, "libvlc_InternalInit");

            vlc_log("[VLC] libvlc_InternalCreate export -> %p\n",
                    (void *)pfn_ic);
            vlc_log("[VLC] libvlc_InternalInit   export -> %p\n",
                    (void *)pfn_ii);

            if (pfn_ic)
            {
                void *t_obj = nullptr;
                __try { t_obj = pfn_ic(); }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    vlc_log("[VLC] libvlc_InternalCreate SEH exception: "
                            "0x%08lX\n",
                            (unsigned long)GetExceptionCode());
                }
                vlc_log("[VLC] libvlc_InternalCreate() -> %p\n", t_obj);

                if (t_obj != nullptr && pfn_ii != nullptr)
                {
                    // InternalCreate succeeded: try InternalInit with zero
                    // args so VLC reads VLC_PLUGIN_PATH and does a real
                    // plugin scan.  (Previously we passed --no-plugins-scan
                    // here, which always produces VLC_ENOMOD = -4 because
                    // the "logger" module is an external plugin, not a
                    // built-in.  That was uninformative.)
                    //
                    // At this point VLC_PLUGIN_PATH points to the SYSTEM
                    // VLC plugins directory (set by the system-plugins
                    // attempt above), which matches the libvlccore.dll
                    // checksum exactly.  If this succeeds, the failure in
                    // the earlier bundled-plugins attempts was a plugin-API
                    // version mismatch between our bundled libvlccore.dll
                    // and the bundled plugin DLLs.
                    int t_rc = -999;
                    __try { t_rc = pfn_ii(t_obj, 0, nullptr); }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        vlc_log("[VLC] libvlc_InternalInit SEH exception: "
                                "0x%08lX\n",
                                (unsigned long)GetExceptionCode());
                    }
                    vlc_log("[VLC] libvlc_InternalInit(0,null) -> %d  "
                            "errmsg: %s\n",
                            t_rc,
                            libvlc_errmsg() ? libvlc_errmsg() : "(null)");
                    // Intentional leak — see comment above.
                }
            }
            else
            {
                vlc_log("[VLC] libvlc_InternalCreate not exported by "
                        "libvlccore.dll — cannot split failure\n");
            }
        }
        else
        {
            vlc_log("[VLC] InternalCreate diagnostic: libvlccore.dll "
                    "not loaded\n");
        }

        // Done with all diagnostic attempts — remove the VEH.
        if (t_veh)
        {
            RemoveVectoredExceptionHandler(t_veh);
            t_veh = nullptr;
            vlc_log("[VLC] VEH removed\n");
        }

        vlc_log("[VLC] EnsureVLCInstance FAILED\n");
    }
    else if (t_veh)
    {
        // libvlc_new succeeded on one of the earlier attempts — clean up VEH.
        RemoveVectoredExceptionHandler(t_veh);
    }
#else
    // Linux — probe for bundled VLC plugins, fall back to system paths.
    //
    // Layout matches Windows/Mac: <exe_dir>/vlc-plugins/plugins/ contains
    // the codec/demux/output .so files.  The AppImage build script copies
    // them from /usr/lib/<arch>/vlc/plugins/.
    //
    // VLC_PLUGIN_PATH is the correct mechanism for both libVLC 3 and 4.
    {
        char t_exe[PATH_MAX];
        ssize_t t_len = readlink("/proc/self/exe", t_exe, sizeof(t_exe) - 1);
        if (t_len > 0)
        {
            t_exe[t_len] = '\0';
            char *t_sep = strrchr(t_exe, '/');
            if (t_sep != nullptr)
                *t_sep = '\0';

            char t_probe[PATH_MAX];
            snprintf(t_probe, sizeof(t_probe),
                     "%s/vlc-plugins/plugins", t_exe);
            if (access(t_probe, F_OK) == 0)
            {
                setenv("VLC_PLUGIN_PATH", t_probe, 1);
            }
            else
            {
                snprintf(t_probe, sizeof(t_probe),
                         "%s/vlc-plugins", t_exe);
                if (access(t_probe, F_OK) == 0)
                    setenv("VLC_PLUGIN_PATH", t_probe, 1);
            }
        }
    }

    const char *t_args[] = {
        "--quiet",
        "--no-osd",
        "--no-stats",
        "--vout=xcb_x11",
    };
    s_vlc_instance = libvlc_new(4, t_args);
#endif

    if (s_vlc_instance == nullptr)
        return false;

    s_vlc_refcount = 1;
    return true;
}

void MCVLCPlayer::ReleaseVLCInstance()
{
    if (s_vlc_refcount == 0)
        return;
    s_vlc_refcount--;
#if defined(TARGET_PLATFORM_WINDOWS) || defined(TARGET_PLATFORM_MACOS_X)
    // Keep the VLC instance alive as a process-wide singleton.
    // libvlc_release tears down VLC's platform video output modules
    // (D3D on Windows, AVFoundation/vout on macOS); re-initializing them
    // via a second libvlc_new in the same process is racy and causes
    // video output failure on the next open (blank frame, no render).
    // The OS reclaims all resources on process exit.
    (void)s_vlc_instance; // intentionally not released
#else
    if (s_vlc_refcount == 0)
    {
        libvlc_release(s_vlc_instance);
        s_vlc_instance = nullptr;
    }
#endif
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

MCVLCPlayer::MCVLCPlayer()
    : m_player(nullptr),
      m_media(nullptr),
      m_view(nullptr),
#if defined(TARGET_PLATFORM_LINUX)
      m_colormap(0),
#endif
      m_rect(MCRectangleMake(0, 0, 0, 0)),
      m_visible(true),
      m_offscreen(false),
      m_looping(false),
      m_play_selection_only(false),
      m_rate(1.0),
      m_volume(100),
      m_selection_start(0),
      m_selection_finish(0),
      m_playing(false),
      m_finished(false),
      m_has_invalid_filename(false),
      m_buffered_time(0),
      m_markers(nullptr),
      m_marker_count(0),
      m_last_marker(UINT64_MAX),
      m_offscreen_bitmap(nullptr),
      m_offscreen_width(0),
      m_offscreen_height(0)
{
    m_offscreen_planes[0] = nullptr;
#if defined(TARGET_PLATFORM_WINDOWS)
    m_play_pending  = false;
    m_view_dead     = false;
    m_destroying    = false;
    m_parent_view   = nullptr;
#endif

    vlc_log("[VLC] MCVLCPlayer::MCVLCPlayer() constructing\n");
    if (!EnsureVLCInstance())
    {
        vlc_log("[VLC] EnsureVLCInstance FAILED\n");
        return;
    }

    m_player = libvlc_media_player_new(s_vlc_instance);
    if (m_player == nullptr)
    {
        vlc_log("[VLC] libvlc_media_player_new FAILED\n");
        ReleaseVLCInstance();
        return;
    }
    vlc_log("[VLC] m_player created: %p\n", (void *)m_player);

    // Subscribe to the events we care about.
    libvlc_event_manager_t *t_em = libvlc_media_player_event_manager(m_player);
    libvlc_event_attach(t_em, libvlc_MediaPlayerPlaying,     OnVLCEvent, this);
    libvlc_event_attach(t_em, libvlc_MediaPlayerEndReached,  OnVLCEvent, this);
    libvlc_event_attach(t_em, libvlc_MediaPlayerTimeChanged, OnVLCEvent, this);

    // Create the platform-specific native view.
#if defined(TARGET_PLATFORM_MACOS_X)
    m_view = MCVLCCreateNSView();
    m_needs_vout_reattach = false;
    m_play_pending_mac    = false;
    m_pending_rate_mac    = 1.0;
#endif
}

MCVLCPlayer::~MCVLCPlayer()
{
#if defined(TARGET_PLATFORM_WINDOWS)
    vlc_log("[VLC] ~MCVLCPlayer() destructing m_view=%p m_play_pending=%d\n",
            m_view, (int)m_play_pending);
#else
    vlc_log("[VLC] ~MCVLCPlayer() destructing m_view=%p\n", m_view);
#endif

    // Stop any pending offscreen decode.
    if (m_offscreen_bitmap != nullptr)
    {
        MCImageFreeBitmap(m_offscreen_bitmap);
        m_offscreen_bitmap = nullptr;
    }

    // Destroy the VLC player (this detaches event listeners automatically).
    if (m_player != nullptr)
    {
#if defined(TARGET_PLATFORM_WINDOWS)
        // Detach VLC from the render surface *before* stopping/releasing.
        // Without this, VLC's D3D renderer holds a dangling HWND reference
        // across the destructor, causing video output failure on the next open.
        libvlc_media_player_set_hwnd(m_player, nullptr);
#elif defined(TARGET_PLATFORM_MACOS_X)
        // NOTE: We intentionally do NOT call set_nsobject(nullptr) here.
        //
        // In earlier versions we called set_nsobject(nullptr) before stop() to
        // detach VLC from the NSView eagerly.  However, VLC 3's macOS vout
        // cleanup block (dispatched to the main queue by set_nsobject(nullptr))
        // contains a bug: it calls free() on an Objective-C object allocated in
        // the ObjC compressible zone (not the default malloc zone), triggering
        // "pointer being freed was not allocated" → SIGABRT.
        //
        // We no longer need set_nsobject(nullptr) to protect the NSView:
        // MCVLCPlayerView's markForDestruction + didRemoveSubview: mechanism
        // keeps the view alive until VLC naturally removes VLCVideoLayerView
        // during stop().  stop() triggers vout teardown via a different (safe)
        // code path that does not exhibit the free()-on-ObjC-object bug.
#endif
        libvlc_media_player_stop(m_player);
        vlc_log("[VLC] ~MCVLCPlayer: stop done\n");
#if defined(TARGET_PLATFORM_MACOS_X)
        // On macOS, VLC 3's vout teardown code (triggered by stop()) dispatches
        // main-queue cleanup blocks that contain memory-management bugs.  Calling
        // libvlc_media_player_release() afterwards triggers malloc free-list
        // inspection that detects those corruptions → SIGABRT.
        //
        // The same reasoning that keeps s_vlc_instance alive applies here:
        // calling release() after stop() is not safe on macOS.  We keep the
        // stopped player alive as a process-wide zombie.  stop() has already
        // freed decoder/demux threads and decoded-picture queues; the residual
        // player struct is a few hundred bytes.  The OS reclaims it on exit.
        vlc_log("[VLC] ~MCVLCPlayer: player kept as zombie (no release) — macOS\n");
#else
        libvlc_media_player_release(m_player);
#endif
        m_player = nullptr;
    }

    // Release the current media.
    if (m_media != nullptr)
    {
        libvlc_media_release(m_media);
        m_media = nullptr;
        vlc_log("[VLC] ~MCVLCPlayer: media release done\n");
    }

    // Destroy the native view.
#if defined(TARGET_PLATFORM_MACOS_X)
    if (m_view != nullptr)
    {
        MCVLCDestroyNSView(m_view);
        m_view = nullptr;
        vlc_log("[VLC] ~MCVLCPlayer: DestroyNSView done\n");
    }
#elif defined(TARGET_PLATFORM_WINDOWS)
    if (m_view != nullptr)
    {
        if (m_view_dead)
        {
            // The HWND was already destroyed externally (cascade from
            // MCNativeLayerWin32).  Nothing left to clean up here.
            vlc_log("[VLC] ~MCVLCPlayer: m_view_dead — skipping DestroyWindow\n");
        }
        else
        {
            // Cancel any deferred-play timer before removing the property so
            // that a WM_TIMER cannot fire after this point with a dangling
            // self pointer.
            if (m_play_pending)
                KillTimer((HWND)m_view, 1001);
            // Remove the window property before DestroyWindow so the WndProc
            // cannot fire with a dangling this pointer during destruction.
            RemovePropA((HWND)m_view, "MCVLCPlayer");
            m_destroying = true;
            DestroyWindow((HWND)m_view);
            m_destroying = false;
        }
        m_view = nullptr;
    }
#elif defined(TARGET_PLATFORM_LINUX)
    if (m_view != nullptr)
    {
        x11::Display *t_dpy = x11::gdk_x11_display_get_xdisplay(
            gdk_display_get_default());
        x11::XDestroyWindow(t_dpy, (x11::Window)(uintptr_t)m_view);
        if (m_colormap != 0)
        {
            x11::XFreeColormap(t_dpy, m_colormap);
            m_colormap = 0;
        }
        m_view = nullptr;
    }
#endif

    MCMemoryDeleteArray(m_markers);
    vlc_log("[VLC] ~MCVLCPlayer: complete\n");
    ReleaseVLCInstance();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void MCVLCPlayer::Realize()
{
    AttachNativeView();
#if defined(TARGET_PLATFORM_MACOS_X)
    // Arm the deferred vout re-attach.  VLC's macOS vout module requires the
    // NSView to be in a live window AND have a non-zero frame when it
    // initializes its OpenGL context.  At this point the view is not yet
    // embedded, so the initial AttachNativeView() call above will not produce
    // working video.  Synchronize() checks this flag and performs a
    // nil → view re-attach once both conditions are met.
    m_needs_vout_reattach = true;
#elif defined(TARGET_PLATFORM_WINDOWS)
    // Unrealize() hides the HWND with SW_HIDE.  Re-show it here so that
    // VLC's D3D vout has a visible surface to render into when Start() is
    // called.  (WS_VISIBLE alone is not enough after an explicit SW_HIDE.)
    if (m_view != nullptr && m_visible)
        ShowWindow((HWND)m_view, SW_SHOWNA);
#endif
    Synchronize();
}

void MCVLCPlayer::Unrealize()
{
    // In offscreen mode there is nothing native to detach.
    if (m_offscreen)
        return;

#if defined(TARGET_PLATFORM_MACOS_X)
    m_needs_vout_reattach = false;
    m_play_pending_mac    = false;  // cancel any pending deferred play
    MCVLCSyncNSView(m_view,
                    MCRectangleMake(0, 0, 0, 0),
                    false);
#elif defined(TARGET_PLATFORM_WINDOWS)
    if (m_view != nullptr)
        ShowWindow((HWND)m_view, SW_HIDE);
#endif
}

void MCVLCPlayer::Synchronize()
{
    if (m_offscreen)
        return;

#if defined(TARGET_PLATFORM_MACOS_X)
    if (m_view != nullptr)
    {
        MCVLCSyncNSView(m_view, m_rect, m_visible);

        // Deferred vout re-attach: VLC's macOS vout (OpenGL) silently fails
        // to initialize when libvlc_media_player_set_nsobject() is called
        // before the NSView is in a live window.  We wait until Synchronize
        // is called with a valid, visible rect AND the view is in a window,
        // then do a nil → view re-attach to force VLC to tear down its
        // degenerate vout state and reinitialize with a properly sized,
        // window-backed view.  This mirrors the Windows nil → hwnd re-attach
        // that is triggered by WM_SIZE.
        if (m_needs_vout_reattach
            && m_player != nullptr
            && m_visible
            && m_rect.width  > 0
            && m_rect.height > 0
            && MCVLCViewHasWindow(m_view))
        {
            m_needs_vout_reattach = false;
            vlc_log("[VLC] Synchronize: deferred vout re-attach (nil → view)\n");
            libvlc_media_player_set_nsobject(m_player, nullptr);
            libvlc_media_player_set_nsobject(m_player, m_view);
        }
    }
#elif defined(TARGET_PLATFORM_WINDOWS)
    // On Windows, MCNativeLayerWin32::doAttach() re-parents m_view into a
    // viewport container HWND and manages geometry + visibility via MoveWindow.
    // Calling SetWindowPos / ShowWindow here (with stack-relative coordinates)
    // would offset the window outside the viewport clipping rect.
    // Do nothing — the native layer owns all geometry after attachplayer().
    (void)m_view;
#elif defined(TARGET_PLATFORM_LINUX)
    if (m_view != nullptr)
    {
        x11::Display *t_dpy = x11::gdk_x11_display_get_xdisplay(
            gdk_display_get_default());
        x11::Window t_win = (x11::Window)(uintptr_t)m_view;
        int t_w = m_rect.width  > 0 ? m_rect.width  : 1;
        int t_h = m_rect.height > 0 ? m_rect.height : 1;
        x11::XMoveResizeWindow(t_dpy, t_win,
                               m_rect.x, m_rect.y, t_w, t_h);
        if (m_visible)
            x11::XMapWindow(t_dpy, t_win);
        else
            x11::XUnmapWindow(t_dpy, t_win);
        x11::XFlush(t_dpy);
    }
#endif
}

#if defined(TARGET_PLATFORM_MACOS_X)
// Fired (async, on the main queue) by MCVLCPlayerView::setFrame: the first
// time its frame becomes non-zero.  At this point MCNativeLayerMac has applied
// the deferred player rect, so VLC's macOS vout can create a properly-sized
// GL surface.  We do the nil → view re-attach to ensure a clean vout init, then
// start play.
/*static*/ void MCVLCPlayer::OnFrameReady(void *p_opaque)
{
    MCVLCPlayer *self = static_cast<MCVLCPlayer *>(p_opaque);
    if (self == nullptr || !self->m_play_pending_mac)
        return;
    if (self->m_player == nullptr || self->m_view == nullptr)
        return;

    self->m_play_pending_mac = false;
    vlc_log("[VLC] OnFrameReady: frame is now non-zero — starting deferred play\n");
    libvlc_media_player_set_nsobject(self->m_player, nullptr);
    libvlc_media_player_set_nsobject(self->m_player, self->m_view);
    libvlc_media_player_play(self->m_player);
    libvlc_media_player_set_rate(self->m_player, (float)self->m_pending_rate_mac);
}
#endif

void MCVLCPlayer::AttachNativeView()
{
    if (m_player == nullptr || m_offscreen)
        return;

#if defined(TARGET_PLATFORM_MACOS_X)
    if (m_view != nullptr)
        libvlc_media_player_set_nsobject(m_player, m_view);
#elif defined(TARGET_PLATFORM_WINDOWS)
    if (m_view != nullptr)
        libvlc_media_player_set_hwnd(m_player, m_view);
#elif defined(TARGET_PLATFORM_LINUX)
    if (m_view != nullptr)
        libvlc_media_player_set_xwindow(m_player,
                                        (uint32_t)(uintptr_t)m_view);
#endif
}

// ---------------------------------------------------------------------------
// GetNativeView / SetNativeParentView
// ---------------------------------------------------------------------------

bool MCVLCPlayer::GetNativeView(void *&r_view)
{
    if (m_view == nullptr)
        return false;
#if defined(TARGET_PLATFORM_WINDOWS)
    if (m_view_dead)
    {
        vlc_log("[VLC] GetNativeView: m_view_dead — recreating native view\n");
        RecreateNativeView();
        if (m_view_dead)
        {
            // Recreation failed.
            vlc_log("[VLC] GetNativeView: RecreateNativeView() FAILED\n");
            return false;
        }
    }
#endif
    r_view = m_view;
    return true;
}

#if defined(TARGET_PLATFORM_WINDOWS)
// ---------------------------------------------------------------------------
// WndProc for the VLC render-surface HWND
// ---------------------------------------------------------------------------
//
// The engine calls SetNativeParentView → Load → Start BEFORE
// MCNativeLayerWin32::doAttach() has finished positioning the window.
// At Start() time the HWND client area is 0×0 and the window has no parent
// yet (MCNativeLayerWin32 calls SetParent(NULL) mid-reparent), so VLC's
// D3D11 vout silently fails to create a swap chain and produces no output.
//
// Fix: when MCNativeLayerWin32 later calls MoveWindow() (which sends
// WM_SIZE), we do a null→hwnd re-attach so VLC tears down any degenerate
// D3D state and creates a fresh swap chain at the correct size.  This fires
// whether libvlc_media_player_play() has already been called or not, and is
// safe to call from any thread.
LRESULT CALLBACK MCVLCPlayer::s_vlc_wnd_proc(HWND hwnd, UINT msg,
                                              WPARAM wp, LPARAM lp)
{
    // Log lifecycle messages so we can see what MCNativeLayerWin32 does to us.
    switch (msg)
    {
        case WM_SIZE:
        case WM_SHOWWINDOW:
        case WM_PARENTNOTIFY:
        case WM_STYLECHANGED:
        {
            RECT t_cr = {};
            GetClientRect(hwnd, &t_cr);
            vlc_log("[VLC] wndproc msg=0x%04X wp=0x%llX lp=0x%llX "
                    "parent=%p client=(%ldx%ld) visible=%d\n",
                    msg, (unsigned long long)wp, (unsigned long long)lp,
                    (void *)GetParent(hwnd),
                    t_cr.right - t_cr.left, t_cr.bottom - t_cr.top,
                    (int)IsWindowVisible(hwnd));
            break;
        }
        case WM_DESTROY:
        {
            MCVLCPlayer *self = reinterpret_cast<MCVLCPlayer *>(
                GetPropA(hwnd, "MCVLCPlayer"));
            vlc_log("[VLC] WM_DESTROY hwnd=%p self(prop)=%p m_play_pending=%d m_destroying=%d\n",
                    (void *)hwnd, (void *)self,
                    self ? (int)self->m_play_pending : -1,
                    self ? (int)self->m_destroying   : -1);
            if (self != nullptr)
            {
                if (!self->m_destroying)
                {
                    // External cascade destroy (MCNativeLayerWin32 tore down its
                    // viewport and took our child HWND with it).  Mark the view
                    // dead so GetNativeView() / Start() will recreate it before
                    // the next MCNativeLayerWin32 instance tries to use it.
                    vlc_log("[VLC] WM_DESTROY: external cascade — marking m_view_dead\n");
                    self->m_view_dead = true;
                    // Cancel any pending timer before the HWND becomes invalid.
                    if (self->m_play_pending)
                    {
                        KillTimer(hwnd, 1001);
                        // Leave m_play_pending = true: Start() will retry once
                        // RecreateNativeView() hands us a live HWND.
                    }
                }
                // Always remove the property — the window is going away.
                RemovePropA(hwnd, "MCVLCPlayer");
            }
            break;
        }
        default: break;
    }

    if (msg == WM_SIZE)
    {
        int t_w = LOWORD(lp);
        int t_h = HIWORD(lp);
        if (t_w > 0 && t_h > 0)
        {
            // Use a private window property instead of GWLP_USERDATA: VLC's
            // D3D vout overwrites GWLP_USERDATA when libvlc_media_player_set_hwnd
            // is called, which would cause self=nullptr here and silently
            // suppress both deferred-play triggering and vout reinit.
            MCVLCPlayer *self = reinterpret_cast<MCVLCPlayer *>(
                GetPropA(hwnd, "MCVLCPlayer"));
            vlc_log("[VLC] WM_SIZE(%dx%d): self(prop)=%p GWLP_USERDATA=%p\n",
                    t_w, t_h, (void *)self,
                    (void *)(uintptr_t)GetWindowLongPtrA(hwnd, GWLP_USERDATA));
            if (self != nullptr && self->m_player != nullptr)
            {
                if (self->m_play_pending)
                {
                    // Deferred play: the render surface now has a valid size.
                    // Kill the fallback timer, reattach VLC to the live HWND,
                    // then start playback.
                    vlc_log("[VLC] WM_SIZE(%dx%d) — triggering deferred play\n",
                            t_w, t_h);
                    KillTimer(hwnd, 1001);
                    libvlc_media_player_set_hwnd(self->m_player, nullptr);
                    libvlc_media_player_set_hwnd(self->m_player, hwnd);
                    libvlc_media_player_play(self->m_player);
                    libvlc_media_player_set_rate(self->m_player,
                                                 (float)self->m_rate);
                    self->m_playing      = true;
                    self->m_play_pending = false;
                }
                else
                {
                    // Not waiting to start: just force a VLC vout reattach so
                    // the D3D swap chain is recreated at the new dimensions.
                    vlc_log("[VLC] WM_SIZE(%dx%d) — forcing VLC vout reinit\n",
                            t_w, t_h);
                    libvlc_media_player_set_hwnd(self->m_player, nullptr);
                    libvlc_media_player_set_hwnd(self->m_player, hwnd);
                }
            }
        }
    }
    else if (msg == WM_TIMER && wp == 1001)
    {
        // Fallback: WM_SIZE with valid dims has not fired yet.  Check whether
        // the window finally has a non-zero client area; if so, start playing.
        MCVLCPlayer *self = reinterpret_cast<MCVLCPlayer *>(
            GetPropA(hwnd, "MCVLCPlayer"));
        vlc_log("[VLC] WM_TIMER(1001): self(prop)=%p GWLP_USERDATA=%p\n",
                (void *)self,
                (void *)(uintptr_t)GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        if (self != nullptr && self->m_play_pending && self->m_player != nullptr)
        {
            RECT t_cr = {};
            GetClientRect(hwnd, &t_cr);
            int t_w = t_cr.right  - t_cr.left;
            int t_h = t_cr.bottom - t_cr.top;
            vlc_log("[VLC] WM_TIMER(1001) hwnd client=(%dx%d)\n", t_w, t_h);
            if (t_w > 0 && t_h > 0)
            {
                KillTimer(hwnd, 1001);
                libvlc_media_player_set_hwnd(self->m_player, nullptr);
                libvlc_media_player_set_hwnd(self->m_player, hwnd);
                libvlc_media_player_play(self->m_player);
                libvlc_media_player_set_rate(self->m_player, (float)self->m_rate);
                self->m_playing      = true;
                self->m_play_pending = false;
            }
            // If still 0×0, the timer fires again after the same interval.
        }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}
#endif // TARGET_PLATFORM_WINDOWS

bool MCVLCPlayer::SetNativeParentView(void *p_parent_view)
{
    // If VLC failed to initialise (DLL missing, plugin path wrong, etc.)
    // m_player is nullptr.  Nothing useful can be done — return false so the
    // engine knows the player is non-functional rather than crashing below.
    if (m_player == nullptr)
        return false;

#if defined(TARGET_PLATFORM_WINDOWS)
    // Create a child window hosted inside the stack window.
    if (p_parent_view == nullptr)
        return false;

    // Register a dedicated window class for the VLC render surface.
    // s_vlc_wnd_proc handles WM_SIZE to force a VLC vout reattach when
    // MCNativeLayerWin32 resizes the window after initial creation.
    // Using "STATIC" is wrong — VLC's D3D renderer requires CS_OWNDC and a
    // real HINSTANCE.  Register our own class exactly once, the same way the
    // DirectShow player does it.
    static bool s_class_registered = false;
    static const char * const k_vlc_class = "HXTVLCVideoWindow";
    if (!s_class_registered)
    {
        HINSTANCE t_inst = (HINSTANCE)GetModuleHandleA(nullptr);
        WNDCLASSEX wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc   = s_vlc_wnd_proc;
        wc.hInstance     = t_inst;
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // black background
        wc.lpszClassName = k_vlc_class;
        RegisterClassEx(&wc);
        s_class_registered = true;
    }

    HWND t_parent = (HWND)p_parent_view;
    HINSTANCE t_inst = (HINSTANCE)GetModuleHandleA(nullptr);

    // Remember the parent so RecreateNativeView() can re-parent a fresh HWND
    // when the old one was cascade-destroyed by MCNativeLayerWin32.
    m_parent_view = p_parent_view;

    vlc_log("[VLC] SetNativeParentView: parent=%p inst=%p\n",
            (void *)t_parent, (void *)t_inst);

    // VLC needs at least 1×1 — do not create a zero-size window.
    int t_w = m_rect.width  > 0 ? m_rect.width  : 1;
    int t_h = m_rect.height > 0 ? m_rect.height : 1;

    // WS_VISIBLE ensures the surface is live before VLC's renderer queries it.
    DWORD t_style = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE;
    HWND t_child = CreateWindowExA(0, k_vlc_class, nullptr,
                                   t_style,
                                   m_rect.x, m_rect.y,
                                   t_w, t_h,
                                   t_parent, nullptr, t_inst, nullptr);
    vlc_log("[VLC] CreateWindowExA -> %p  (err=%lu)  m_rect=(%d,%d,%dx%d)\n",
            (void *)t_child, (unsigned long)GetLastError(),
            m_rect.x, m_rect.y, m_rect.width, m_rect.height);
    if (t_child == nullptr)
        return false;

    // Store `this` in a named window property so the WndProc can reach the
    // player instance when WM_SIZE / WM_TIMER fires.  We use SetPropA rather
    // than GWLP_USERDATA because VLC's D3D vout overwrites GWLP_USERDATA when
    // libvlc_media_player_set_hwnd() is called, silently making self=nullptr
    // in every WndProc handler that depended on GWLP_USERDATA.
    SetPropA(t_child, "MCVLCPlayer", (HANDLE)reinterpret_cast<ULONG_PTR>(this));
    {
        // Log immediate post-creation state.
        RECT t_imm = {};
        GetClientRect(t_child, &t_imm);
        vlc_log("[VLC] post-create: parent=%p  client=(%ldx%ld)  visible=%d\n",
                (void *)GetParent(t_child),
                t_imm.right - t_imm.left, t_imm.bottom - t_imm.top,
                (int)IsWindowVisible(t_child));
    }

    // Show or hide based on the current visibility state.
    if (!m_visible)
        ShowWindow(t_child, SW_HIDE);

    if (m_view != nullptr)
    {
        // Remove the window property before destroying the old surface so the
        // WndProc cannot fire with a dangling this pointer during destruction.
        RemovePropA((HWND)m_view, "MCVLCPlayer");
        // Detach VLC before destroying the old surface, otherwise VLC's D3D
        // renderer holds a dangling reference and corrupts state for the new one.
        libvlc_media_player_set_hwnd(m_player, nullptr);
        // Signal WM_DESTROY that this is our own intentional teardown, not an
        // external cascade, so it doesn't set m_view_dead.
        m_destroying = true;
        DestroyWindow((HWND)m_view);
        m_destroying = false;
    }

    // Reset the dead flag — we now have a fresh HWND.
    m_view_dead = false;
    m_view = t_child;
    // Give VLC the new window handle now that the surface is ready.
    libvlc_media_player_set_hwnd(m_player, m_view);
    vlc_log("[VLC] HWND %p given to libvlc_media_player_set_hwnd\n",
            (void *)m_view);
#elif defined(TARGET_PLATFORM_LINUX)
    vlc_log("[VLC] SetNativeParentView: parent=%p rect=(%d,%d,%dx%d)\n",
            p_parent_view, m_rect.x, m_rect.y, m_rect.width, m_rect.height);

    if (p_parent_view == nullptr)
    {
        vlc_log("[VLC] SetNativeParentView: parent is NULL\n");
        return false;
    }

    // Use raw X11 child window instead of GDK — VLC's xcb video output
    // does not render into GDK-managed windows.
    GdkWindow *t_gdk_parent = (GdkWindow *)p_parent_view;
    x11::Window t_parent_xid = x11::gdk_x11_drawable_get_xid(
        (GdkDrawable *)t_gdk_parent);
    x11::Display *t_dpy = x11::gdk_x11_display_get_xdisplay(
        gdk_display_get_default());

    int t_x = m_rect.x;
    int t_y = m_rect.y;
    int t_w = m_rect.width  > 0 ? m_rect.width  : 1;
    int t_h = m_rect.height > 0 ? m_rect.height : 1;

    if (m_view != nullptr)
    {
        x11::XDestroyWindow(t_dpy, (x11::Window)(uintptr_t)m_view);
        m_view = nullptr;
    }
    if (m_colormap != 0)
    {
        x11::XFreeColormap(t_dpy, m_colormap);
        m_colormap = 0;
    }

    // Use a 24-bit (non-ARGB) visual to prevent transparency under
    // compositing WMs.  GDK may use a 32-bit ARGB visual for its
    // windows; child windows inherit that visual, and VLC's xcb output
    // leaves the alpha byte at 0 — making every pixel transparent.
    x11::Window t_win = 0;
    x11::XVisualInfo t_vinfo;
    int t_screen = x11::XDefaultScreen(t_dpy);
    if (x11::XMatchVisualInfo(t_dpy, t_screen, 24, TrueColor, &t_vinfo))
    {
        m_colormap = x11::XCreateColormap(
            t_dpy, t_parent_xid, t_vinfo.visual, AllocNone);
        x11::XSetWindowAttributes t_attrs;
        memset(&t_attrs, 0, sizeof(t_attrs));
        t_attrs.background_pixel = 0;
        t_attrs.border_pixel = 0;
        t_attrs.colormap = m_colormap;
        t_win = x11::XCreateWindow(
            t_dpy, t_parent_xid, t_x, t_y, t_w, t_h,
            0, t_vinfo.depth, InputOutput, t_vinfo.visual,
            CWBackPixel | CWBorderPixel | CWColormap, &t_attrs);
    }
    else
    {
        t_win = x11::XCreateSimpleWindow(
            t_dpy, t_parent_xid, t_x, t_y, t_w, t_h, 0, 0, 0);
    }

    if (t_win == 0)
    {
        if (m_colormap != 0)
        {
            x11::XFreeColormap(t_dpy, m_colormap);
            m_colormap = 0;
        }
        return false;
    }

    x11::XMapWindow(t_dpy, t_win);
    x11::XFlush(t_dpy);

    m_view = (void *)(uintptr_t)t_win;

    vlc_log("[VLC] SetNativeParentView: xid=%lu view=%p\n",
            (unsigned long)t_win, m_view);
    libvlc_media_player_set_xwindow(m_player, (uint32_t)t_win);
#elif defined(TARGET_PLATFORM_MACOS_X)
    // Add the player view as a subview of the stack window's content view.
    MCVLCReparentNSView(m_view, p_parent_view);
    Synchronize();
#endif

    return true;
}

#if defined(TARGET_PLATFORM_WINDOWS)
// ---------------------------------------------------------------------------
// RecreateNativeView — rebuild the render-surface HWND after an external
// cascade destruction (MCNativeLayerWin32 tore down its viewport container).
// ---------------------------------------------------------------------------
//
// The engine create→destroy→recreate lifecycle (MCNativeLayerWin32 is
// destroyed and re-created during card open) means our child HWND can be
// taken out from under us before Start() is called.  When that happens
// WM_DESTROY sets m_view_dead.  The next call to GetNativeView() or Start()
// detects this and calls here to build a fresh child HWND so that the second
// MCNativeLayerWin32 instance gets a live surface to attach to.
void MCVLCPlayer::RecreateNativeView()
{
    if (m_parent_view == nullptr)
    {
        vlc_log("[VLC] RecreateNativeView: no saved parent — cannot recreate\n");
        return;
    }

    static const char * const k_vlc_class = "HXTVLCVideoWindow";
    HWND t_parent = (HWND)m_parent_view;
    HINSTANCE t_inst = (HINSTANCE)GetModuleHandleA(nullptr);

    // Use the stored rect if available; fall back to 1×1 so VLC can attach.
    int t_w = m_rect.width  > 0 ? m_rect.width  : 1;
    int t_h = m_rect.height > 0 ? m_rect.height : 1;

    DWORD t_style = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE;
    HWND t_child = CreateWindowExA(0, k_vlc_class, nullptr,
                                   t_style,
                                   m_rect.x, m_rect.y,
                                   t_w, t_h,
                                   t_parent, nullptr, t_inst, nullptr);
    vlc_log("[VLC] RecreateNativeView: CreateWindowExA -> %p (err=%lu) "
            "parent=%p rect=(%d,%d,%dx%d)\n",
            (void *)t_child, (unsigned long)GetLastError(),
            (void *)t_parent,
            m_rect.x, m_rect.y, t_w, t_h);

    if (t_child == nullptr)
        return;  // m_view_dead stays true; callers will handle the failure

    SetPropA(t_child, "MCVLCPlayer", (HANDLE)reinterpret_cast<ULONG_PTR>(this));

    if (!m_visible)
        ShowWindow(t_child, SW_HIDE);

    // The old HWND (m_view) is already dead — don't touch it.
    // Update m_view to the new surface and hand it to VLC.
    m_view     = t_child;
    m_view_dead = false;

    // Detach VLC from the dead handle (in case it still holds it), then
    // attach to the new one.
    libvlc_media_player_set_hwnd(m_player, nullptr);
    libvlc_media_player_set_hwnd(m_player, m_view);

    vlc_log("[VLC] RecreateNativeView: new HWND %p given to VLC\n",
            (void *)m_view);
}
#endif // TARGET_PLATFORM_WINDOWS

// ---------------------------------------------------------------------------
// Load / Unload
// ---------------------------------------------------------------------------

void MCVLCPlayer::Load(MCStringRef p_filename, bool p_is_url)
{
    Unload();

    if (m_player == nullptr || MCStringIsEmpty(p_filename))
        return;

    MCAutoStringRefAsCString t_cstr;
    if (!t_cstr.Lock(p_filename))
        return;

#if defined(TARGET_PLATFORM_WINDOWS)
    {
        HWND t_hw = (HWND)m_view;
        RECT t_cr = {};
        if (t_hw) GetClientRect(t_hw, &t_cr);
        vlc_log("[VLC] Load entry: hwnd=%p parent=%p client=(%ldx%ld) visible=%d\n",
                (void *)t_hw, (void *)(t_hw ? GetParent(t_hw) : nullptr),
                t_cr.right - t_cr.left, t_cr.bottom - t_cr.top,
                (int)(t_hw ? IsWindowVisible(t_hw) : FALSE));
    }
#endif
    vlc_log("[VLC] Load: '%s' (url=%d)\n", *t_cstr, (int)p_is_url);

    if (p_is_url)
    {
        m_media = libvlc_media_new_location(s_vlc_instance, *t_cstr);
    }
    else
    {
        // Prefer libvlc_media_new_path — it handles the path→URI conversion
        // internally via vlc_path2uri().  On Windows that function calls
        // MultiByteToWideChar(CP_ACP, ...) which occasionally returns null
        // in non-standard process environments, causing new_path to return
        // null even for a valid ASCII path.  If that happens, fall back to
        // constructing the file:/// URI ourselves and using new_location,
        // which accepts a pre-formed URI and skips the conversion entirely.
        m_media = libvlc_media_new_path(s_vlc_instance, *t_cstr);
        vlc_log("[VLC] libvlc_media_new_path -> %p  errmsg: %s\n",
                (void *)m_media,
                m_media ? "ok"
                        : (libvlc_errmsg() ? libvlc_errmsg() : "(null)"));

#if defined(TARGET_PLATFORM_WINDOWS)
        if (m_media == nullptr)
        {
            // Build a file:/// URI manually.
            // • Replace every backslash with a forward slash.
            // • Percent-encode space (0x20) and any byte > 0x7E.
            // • Prepend "file:///" (three slashes: two for authority, one
            //   before the drive letter — correct for absolute paths like
            //   C:/...).
            const char *t_src = *t_cstr;
            // Worst case: every byte → %XX (3 chars) + "file:///" + NUL.
            size_t t_uri_cap = 8 + strlen(t_src) * 3 + 1;
            char  *t_uri = (char *)malloc(t_uri_cap);
            if (t_uri)
            {
                char *t_dst = t_uri;
                memcpy(t_dst, "file:///", 8); t_dst += 8;
                for (const char *p = t_src; *p; p++)
                {
                    unsigned char c = (unsigned char)*p;
                    if (c == '\\')
                    {
                        *t_dst++ = '/';
                    }
                    else if (c == ' ' || c > 0x7E ||
                             c == '#' || c == '?' || c == '%')
                    {
                        // Percent-encode
                        static const char hex[] = "0123456789ABCDEF";
                        *t_dst++ = '%';
                        *t_dst++ = hex[c >> 4];
                        *t_dst++ = hex[c & 0xF];
                    }
                    else
                    {
                        *t_dst++ = (char)c;
                    }
                }
                *t_dst = '\0';

                vlc_log("[VLC] fallback URI: '%s'\n", t_uri);
                m_media = libvlc_media_new_location(s_vlc_instance, t_uri);
                vlc_log("[VLC] libvlc_media_new_location (fallback) -> %p  "
                        "errmsg: %s\n",
                        (void *)m_media,
                        m_media ? "ok"
                                : (libvlc_errmsg() ? libvlc_errmsg()
                                                   : "(null)"));
                free(t_uri);
            }
        }
#endif // TARGET_PLATFORM_WINDOWS
    }

    if (m_media == nullptr)
    {
        vlc_log("[VLC] libvlc_media_new_* FAILED for '%s'\n", *t_cstr);
        m_has_invalid_filename = true;
        return;
    }

    libvlc_media_player_set_media(m_player, m_media);
    vlc_log("[VLC] media set OK\n");

    // Parse the media synchronously so track metadata (dimensions, duration)
    // is immediately available when the engine queries MovieRect / Duration
    // right after setting the filename.  libvlc_media_parse is deprecated in
    // favour of the async libvlc_media_parse_with_options, but for local files
    // the blocking version is acceptable and keeps the call-site simple.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996) // libvlc_media_parse: deprecated
    libvlc_media_parse(m_media);
#pragma warning(pop)
#else
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    libvlc_media_parse(m_media);
#pragma clang diagnostic pop
#endif


    // Reset selection / state.
    m_selection_start  = 0;
    m_selection_finish = 0;
    m_last_marker      = UINT64_MAX;
    m_buffered_time    = 0;
    m_finished         = false;
    m_has_invalid_filename = false;

    // Wire up the native view (or offscreen callbacks) now that we have media.
    if (!m_offscreen)
    {
#if defined(TARGET_PLATFORM_WINDOWS)
        // Force VLC to reinitialize its D3D video output via a null→hwnd
        // transition.  VLC's vout module is lazy: it only creates the D3D
        // device + swap chain on the first rendered frame, and it skips that
        // work if it thinks nothing has changed.  Without this null cycle,
        // the very first play attempt on a fresh player produces no video
        // because VLC detects "same HWND as before" and reuses a vout state
        // that was never properly initialized.  Unload() has already called
        // libvlc_media_player_stop(), so the player is in a clean stopped
        // state — the null transition is completely safe here.
        if (m_player != nullptr && m_view != nullptr)
        {
            libvlc_media_player_set_hwnd(m_player, nullptr);
            libvlc_media_player_set_hwnd(m_player, (HWND)m_view);
            vlc_log("[VLC] Load: null->hwnd cycle done (%p)\n", m_view);
        }
        else
        {
            AttachNativeView();
        }
#else
        AttachNativeView();
#endif
    }
    else
        SetupOffscreenCallbacks();

    // Apply current volume.
    libvlc_audio_set_volume(m_player, m_volume);
}

void MCVLCPlayer::Unload()
{
#if defined(TARGET_PLATFORM_WINDOWS)
    // If a deferred-play timer is outstanding, cancel it before stopping the
    // player so that WM_TIMER cannot call libvlc_media_player_play on a media
    // that is being torn down.
    if (m_play_pending && m_view != nullptr)
    {
        KillTimer((HWND)m_view, 1001);
        m_play_pending = false;
    }
#endif

    if (m_player != nullptr)
        libvlc_media_player_stop(m_player);

    if (m_media != nullptr)
    {
        libvlc_media_release(m_media);
        m_media = nullptr;
    }

    m_playing  = false;
    m_finished = false;
}

// ---------------------------------------------------------------------------
// Offscreen rendering setup
// ---------------------------------------------------------------------------

void MCVLCPlayer::SetupOffscreenCallbacks()
{
    if (m_player == nullptr)
        return;

    libvlc_video_set_callbacks(m_player,
                               OnVideoLock,
                               OnVideoUnlock,
                               OnVideoDisplay,
                               this);

    libvlc_video_set_format_callbacks(m_player,
                                      OnVideoFormat,
                                      OnVideoCleanup);
}

// ---------------------------------------------------------------------------
// Playback control
// ---------------------------------------------------------------------------

bool MCVLCPlayer::IsPlaying()
{
    if (m_player == nullptr)
        return false;
    return libvlc_media_player_get_state(m_player) == libvlc_Playing;
}

void MCVLCPlayer::Start(double rate)
{
#if defined(TARGET_PLATFORM_WINDOWS)
    {
        // Log actual window geometry and parent chain so we can see whether
        // VLC is being given a properly-sized, visible surface to render into.
        HWND t_hw = (HWND)m_view;
        RECT t_cr = {0};
        if (t_hw) GetClientRect(t_hw, &t_cr);
        HWND t_par = t_hw ? GetParent(t_hw) : nullptr;
        RECT t_pr = {0};
        if (t_par) GetClientRect(t_par, &t_pr);
        BOOL t_vis = t_hw ? IsWindowVisible(t_hw) : FALSE;
        BOOL t_par_vis = t_par ? IsWindowVisible(t_par) : FALSE;
        vlc_log("[VLC] Start(rate=%.2f) m_player=%p m_view=%p\n"
                "[VLC]   hwnd client=(%ldx%ld) visible=%d\n"
                "[VLC]   parent=%p client=(%ldx%ld) visible=%d\n",
                rate, (void *)m_player, m_view,
                t_cr.right - t_cr.left, t_cr.bottom - t_cr.top, (int)t_vis,
                (void *)t_par,
                t_pr.right - t_pr.left, t_pr.bottom - t_pr.top,
                (int)t_par_vis);
    }
#elif defined(TARGET_PLATFORM_MACOS_X)
    vlc_log("[VLC] Start(rate=%.2f) m_player=%p m_view=%p inWindow=%d\n",
            rate, (void *)m_player, m_view, (int)MCVLCViewHasWindow(m_view));
#else
    vlc_log("[VLC] Start(rate=%.2f) m_player=%p m_view=%p\n",
            rate, (void *)m_player, m_view);
#endif
    if (m_player == nullptr)
        return;

    m_rate = rate;

    // Rewind if at the end and playing forward.
    if (m_finished && rate > 0)
    {
        libvlc_media_player_set_time(m_player, 0);
        m_finished = false;
    }

#if defined(TARGET_PLATFORM_WINDOWS)
    // When using a native HWND for rendering, VLC's D3D vout creates its swap
    // chain on the first decoded frame.  If the HWND has a zero client area at
    // that point (because MCNativeLayerWin32::doAttach fires with m_rect=0×0
    // before the real geometry is applied) the swap chain creation fails
    // permanently and video never appears.
    //
    // Guard: if the render surface is still 0×0, arm a timer and return.
    // s_vlc_wnd_proc will call libvlc_media_player_play once WM_SIZE or the
    // fallback WM_TIMER delivers a valid client area.
    // If the HWND was cascade-destroyed externally (MCNativeLayerWin32 tore
    // down its viewport), recreate it now so that the new layer instance gets
    // a live render surface and VLC can attach its D3D vout.
    if (!m_offscreen && m_view != nullptr && m_view_dead)
    {
        vlc_log("[VLC] Start: m_view_dead — recreating native view before play\n");
        RecreateNativeView();
        if (m_view_dead)
        {
            vlc_log("[VLC] Start: RecreateNativeView() FAILED\n");
            return;
        }
    }

    if (!m_offscreen && m_view != nullptr)
    {
        // Audio-only media (e.g. MP3) has no video track, so the render HWND
        // always has zero client height.  The D3D vout size guard below would
        // arm the WM_TIMER loop forever and never play.  Audio output doesn't
        // need an HWND, so detect this case and play immediately.
        bool t_has_video = false;
        if (m_media != nullptr)
        {
            libvlc_media_track_t **t_tracks = nullptr;
            unsigned t_count = libvlc_media_tracks_get(m_media, &t_tracks);
            for (unsigned i = 0; i < t_count; i++)
            {
                if (t_tracks[i]->i_type == libvlc_track_video)
                {
                    t_has_video = true;
                    break;
                }
            }
            if (t_tracks != nullptr)
                libvlc_media_tracks_release(t_tracks, t_count);
        }

        HWND t_hw = (HWND)m_view;
        RECT t_cr = {0};
        GetClientRect(t_hw, &t_cr);
        if (!t_has_video || (t_cr.right == 0 && t_cr.bottom == 0))
        {
            if (!t_has_video)
            {
                // Audio-only: no D3D surface needed, play immediately.
                vlc_log("[VLC] Start: audio-only media — playing directly\n");
                libvlc_media_player_play(m_player);
                libvlc_media_player_set_rate(m_player, (float)rate);
                m_playing = true;
                return;
            }
            // Video with zero-size HWND: defer until WM_SIZE delivers valid dims.
            m_play_pending = true;
            // m_rate was already stored above; the WndProc will use it.
            UINT_PTR t_timer = SetTimer(t_hw, 1001, 50, nullptr);
            vlc_log("[VLC] Start: HWND is 0x0 — deferring play, "
                    "SetTimer(%p,1001,50) -> %llu\n",
                    (void *)t_hw, (unsigned long long)t_timer);
            m_playing = true;  // record that the caller wants playback
            return;
        }
    }
#endif

#if defined(TARGET_PLATFORM_LINUX)
    if (m_view != nullptr)
    {
        uint32_t t_xid = (uint32_t)(uintptr_t)m_view;
        libvlc_media_player_set_xwindow(m_player, t_xid);
        vlc_log("[VLC] Start: re-set xwindow=%u\n", t_xid);
    }
#endif

#if defined(TARGET_PLATFORM_MACOS_X)
    // On macOS, libvlc_media_player_set_nsobject() is NOT called in the normal
    // open flow (MCNativeLayerMac sets the view's frame/visibility directly,
    // bypassing the MCVLCPlayer API).  We must call it here before play.
    //
    // Critical timing issue: MCNativeLayer::m_defer_geometry_changes starts
    // true and m_rect starts {0,0,0,0}.  doAttach() calls doSetGeometry(0,0,0,0)
    // — so the NSView's frame is ZERO when open() → attachplayer() → Start()
    // fires.  The actual player rect lives in m_deferred_rect and is applied
    // asynchronously during the first OnPaint() cycle.  If we call
    // libvlc_media_player_play() now, VLC's macosx vout creates a zero-size GL
    // surface and silently fails to render video (audio still works because it
    // uses a separate output pipeline that doesn't need a view).
    //
    // Fix: if the view's frame is non-zero we play immediately.  Otherwise we
    // arm a one-shot setFrame: callback on MCVLCPlayerView; when OnPaint()
    // applies the deferred rect and calls [view setFrame: actualRect], the
    // callback fires (async, to avoid re-entering AppKit's layout pass) and
    // starts VLC at that safe moment.
    //
    // Audio-only exception: for media with no video tracks (e.g. MP3), the
    // player's video area has zero height — doSetGeometry is always called with
    // a zero-height rect, so the NSView frame is perpetually zero and the
    // frame-ready callback never fires.  Audio output in VLC uses a completely
    // separate pipeline that doesn't need an NSObject view, so we can skip the
    // deferral and play directly.
    if (!m_offscreen && m_view != nullptr)
    {
        // Detect audio-only media: check parsed track list for any video track.
        bool t_has_video = false;
        if (m_media != nullptr)
        {
            libvlc_media_track_t **t_tracks = nullptr;
            unsigned t_count = libvlc_media_tracks_get(m_media, &t_tracks);
            for (unsigned i = 0; i < t_count; i++)
            {
                if (t_tracks[i]->i_type == libvlc_track_video)
                {
                    t_has_video = true;
                    break;
                }
            }
            if (t_tracks != nullptr)
                libvlc_media_tracks_release(t_tracks, t_count);
        }

        bool t_has_frame  = MCVLCViewHasNonZeroFrame(m_view);
        bool t_has_window = MCVLCViewHasWindow(m_view);
        vlc_log("[VLC] Start: NSView %p inWindow=%d hasFrame=%d hasVideo=%d\n",
                m_view, (int)t_has_window, (int)t_has_frame, (int)t_has_video);

        if (!t_has_video)
        {
            // Audio-only: no view needed, play immediately.
            vlc_log("[VLC] Start: audio-only media — playing directly\n");
            libvlc_media_player_play(m_player);
            libvlc_media_player_set_rate(m_player, (float)rate);
            m_playing = true;
            return;
        }

        // Always defer play to the next run-loop turn via the frame-ready
        // mechanism, regardless of whether the frame is already non-zero.
        //
        // When hasFrame=0 (first open): MCVLCSetFrameReadyCallback arms the
        // setFrame: callback and play happens when the native layer applies its
        // deferred geometry.
        //
        // When hasFrame=1 (reopen): MCVLCSetFrameReadyCallback detects the
        // frame is already valid and dispatches the callback on the next
        // main-queue turn.  The one-turn deferral lets AppKit finish all
        // pending layout and compositing operations before VLC initialises its
        // vout surface.  Playing immediately in this case caused VLC's macOS
        // vout to silently fail even though set_nsobject and the frame were
        // both correct.
        vlc_log("[VLC] Start: deferring play (hasFrame=%d)\n", (int)t_has_frame);
        m_play_pending_mac = true;
        m_pending_rate_mac = rate;
        MCVLCSetFrameReadyCallback(m_view, OnFrameReady, this);
        m_playing = true;
        return;
    }
#endif

    libvlc_media_player_play(m_player);
    libvlc_media_player_set_rate(m_player, (float)rate);
    m_playing = true;
}

void MCVLCPlayer::Stop()
{
    if (m_player == nullptr)
        return;
    libvlc_media_player_pause(m_player);
    m_playing = false;
}

void MCVLCPlayer::Step(int amount)
{
    if (m_player == nullptr)
        return;

    libvlc_time_t t_cur = libvlc_media_player_get_time(m_player);
    libvlc_time_t t_new = t_cur + (libvlc_time_t)amount * 40; // ~40 ms per frame @25fps
    if (t_new < 0)
        t_new = 0;
    libvlc_media_player_set_time(m_player, t_new);
}

// ---------------------------------------------------------------------------
// LockBitmap / UnlockBitmap (offscreen snapshot)
// ---------------------------------------------------------------------------

bool MCVLCPlayer::LockBitmap(const MCGIntegerSize &p_size,
                              MCImageBitmap *&r_bitmap)
{
    if (m_offscreen_bitmap == nullptr)
        return false;

    // Scale the offscreen bitmap to the requested size if needed.
    if ((unsigned)p_size.width  == m_offscreen_width &&
        (unsigned)p_size.height == m_offscreen_height)
    {
        r_bitmap = m_offscreen_bitmap;
        return true;
    }

    // Allocate a scaled copy.
    MCImageBitmap *t_scaled = nullptr;
    if (!MCImageBitmapCreate(p_size.width, p_size.height, t_scaled))
        return false;

    // Simple nearest-neighbour scale.
    for (uint32_t ty = 0; ty < (uint32_t)p_size.height; ty++)
    {
        uint32_t sy = ty * m_offscreen_height / (uint32_t)p_size.height;
        uint32_t *t_src = (uint32_t *)((uint8_t *)m_offscreen_bitmap->data +
                                       sy * m_offscreen_bitmap->stride);
        uint32_t *t_dst = (uint32_t *)((uint8_t *)t_scaled->data +
                                       ty * t_scaled->stride);
        for (uint32_t tx = 0; tx < (uint32_t)p_size.width; tx++)
        {
            uint32_t sx = tx * m_offscreen_width / (uint32_t)p_size.width;
            t_dst[tx] = t_src[sx];
        }
    }

    r_bitmap = t_scaled;
    return true;
}

void MCVLCPlayer::UnlockBitmap(MCImageBitmap *bitmap)
{
    // Free the scaled copy (if any) — but never free m_offscreen_bitmap itself.
    if (bitmap != m_offscreen_bitmap)
        MCImageFreeBitmap(bitmap);
}

// ---------------------------------------------------------------------------
// SetProperty / GetProperty
// ---------------------------------------------------------------------------

void MCVLCPlayer::SetProperty(MCPlatformPlayerProperty p_property,
                               MCPlatformPropertyType   p_type,
                               void                    *p_value)
{
    switch (p_property)
    {
        // --- file / URL ---
        case kMCPlatformPlayerPropertyURL:
            Load(*(MCStringRef *)p_value, true);
            break;

        case kMCPlatformPlayerPropertyFilename:
            Load(*(MCStringRef *)p_value, false);
            break;

        // --- geometry ---
        case kMCPlatformPlayerPropertyRect:
            m_rect = *(MCRectangle *)p_value;
#if defined(TARGET_PLATFORM_WINDOWS)
            vlc_log("[VLC] SetProperty(rect): m_rect=(%d,%d,%dx%d) "
                    "m_play_pending=%d\n",
                    m_rect.x, m_rect.y, m_rect.width, m_rect.height,
                    (int)m_play_pending);
#else
            vlc_log("[VLC] SetProperty(rect): m_rect=(%d,%d,%dx%d)\n",
                    m_rect.x, m_rect.y, m_rect.width, m_rect.height);
#endif
#if defined(TARGET_PLATFORM_WINDOWS)
            // Primary deferred-play trigger: the engine has set the player's
            // bounds.  If a play is waiting on the HWND becoming non-zero, fire
            // it now — the null→hwnd cycle forces VLC's D3D vout to (re)init,
            // and MCNativeLayerWin32 will MoveWindow to the correct size shortly
            // after this returns, at which point WM_SIZE fires the vout reinit.
            if (m_play_pending && m_player != nullptr &&
                m_rect.width > 0 && m_rect.height > 0 && !m_offscreen)
            {
                vlc_log("[VLC] SetProperty(rect): triggering deferred play "
                        "(rect now %dx%d) m_view_dead=%d\n",
                        m_rect.width, m_rect.height, (int)m_view_dead);

                // If the HWND was cascade-destroyed, recreate it before using it.
                if (m_view_dead)
                {
                    RecreateNativeView();
                    if (m_view_dead)
                    {
                        vlc_log("[VLC] SetProperty(rect): RecreateNativeView() "
                                "FAILED — deferring\n");
                        // Leave m_play_pending set; Start() will retry.
                        goto after_rect_play;
                    }
                }

                if (m_view != nullptr)
                    KillTimer((HWND)m_view, 1001);
                if (m_view != nullptr)
                {
                    libvlc_media_player_set_hwnd(m_player, nullptr);
                    libvlc_media_player_set_hwnd(m_player, m_view);
                }
                libvlc_media_player_play(m_player);
                libvlc_media_player_set_rate(m_player, (float)m_rate);
                m_playing      = true;
                m_play_pending = false;
                after_rect_play:;
            }
#endif
            Synchronize();
            break;

        case kMCPlatformPlayerPropertyVisible:
            m_visible = *(bool *)p_value;
            Synchronize();
            break;

        // --- offscreen mode ---
        case kMCPlatformPlayerPropertyOffscreen:
        {
            bool t_new_offscreen = *(bool *)p_value;
            if (t_new_offscreen == m_offscreen)
                break;
            m_offscreen = t_new_offscreen;
#if defined(TARGET_PLATFORM_LINUX)
            if (m_offscreen && m_view != nullptr)
            {
                x11::Display *t_dpy = x11::gdk_x11_display_get_xdisplay(
                    gdk_display_get_default());
                x11::XUnmapWindow(t_dpy, (x11::Window)(uintptr_t)m_view);
                x11::XFlush(t_dpy);
            }
#endif
            if (m_media != nullptr)
            {
                // Re-attach VLC to the correct output.
                libvlc_media_player_stop(m_player);
                if (m_offscreen)
                    SetupOffscreenCallbacks();
                else
                {
                    AttachNativeView();
                    Synchronize();
                }
            }
#if defined(TARGET_PLATFORM_LINUX)
            else if (!m_offscreen)
            {
                Synchronize();
            }
#endif
            break;
        }

        // --- time ---
        case kMCPlatformPlayerPropertyCurrentTime:
            if (m_player != nullptr)
                libvlc_media_player_set_time(m_player,
                    (libvlc_time_t)(*(MCPlatformPlayerDuration *)p_value));
            break;

        case kMCPlatformPlayerPropertyStartTime:
            m_selection_start = *(MCPlatformPlayerDuration *)p_value;
            if (m_selection_start > m_selection_finish)
                m_selection_start = m_selection_finish;
            break;

        case kMCPlatformPlayerPropertyFinishTime:
            m_selection_finish = *(MCPlatformPlayerDuration *)p_value;
            if (m_selection_start > m_selection_finish)
                m_selection_finish = m_selection_start;
            break;

        // --- playback ---
        case kMCPlatformPlayerPropertyPlayRate:
            m_rate = *(double *)p_value;
            if (m_player != nullptr && IsPlaying())
                libvlc_media_player_set_rate(m_player, (float)m_rate);
            break;

        case kMCPlatformPlayerPropertyVolume:
            m_volume = *(uint16_t *)p_value;
            if (m_player != nullptr)
                libvlc_audio_set_volume(m_player, (int)m_volume);
            break;

        case kMCPlatformPlayerPropertyLoop:
            m_looping = *(bool *)p_value;
            break;

        case kMCPlatformPlayerPropertyOnlyPlaySelection:
            m_play_selection_only = *(bool *)p_value;
            break;

        // --- markers ---
        case kMCPlatformPlayerPropertyMarkers:
        {
            MCPlatformPlayerDurationArray *t_arr =
                (MCPlatformPlayerDurationArray *)p_value;
            m_last_marker = UINT64_MAX;
            MCMemoryDeleteArray(m_markers);
            m_markers     = nullptr;
            m_marker_count = 0;
            if (t_arr->count > 0)
            {
                /* UNCHECKED */
                MCMemoryResizeArray(t_arr->count, m_markers, m_marker_count);
                MCMemoryCopy(m_markers, t_arr->ptr,
                             m_marker_count * sizeof(MCPlatformPlayerDuration));
            }
            break;
        }

        // --- ignored / not applicable ---
        case kMCPlatformPlayerPropertyShowSelection:
        case kMCPlatformPlayerPropertyMirrored:
        case kMCPlatformPlayerPropertyScalefactor:
        case kMCPlatformPlayerPropertyLeftBalance:
        case kMCPlatformPlayerPropertyRightBalance:
        case kMCPlatformPlayerPropertyPan:
        default:
            break;
    }
}

void MCVLCPlayer::GetProperty(MCPlatformPlayerProperty p_property,
                               MCPlatformPropertyType   p_type,
                               void                    *r_value)
{
    switch (p_property)
    {
        case kMCPlatformPlayerPropertyOffscreen:
            *(bool *)r_value = m_offscreen;
            break;

        case kMCPlatformPlayerPropertyRect:
            *(MCRectangle *)r_value = m_rect;
            break;

        case kMCPlatformPlayerPropertyVisible:
            *(bool *)r_value = m_visible;
            break;

        case kMCPlatformPlayerPropertyMovieRect:
        {
            unsigned t_w = 0, t_h = 0;
            // libvlc_video_get_size only works during active playback.
            // For the pre-play query from prepare(), read track metadata
            // (populated by libvlc_media_parse called in Load()).
            if (m_player != nullptr)
                libvlc_video_get_size(m_player, 0, &t_w, &t_h);
            if (t_w == 0 && t_h == 0 && m_media != nullptr)
            {
                libvlc_media_track_t **t_tracks = nullptr;
                unsigned t_count = libvlc_media_tracks_get(m_media, &t_tracks);
                for (unsigned i = 0; i < t_count; i++)
                {
                    if (t_tracks[i]->i_type == libvlc_track_video &&
                        t_tracks[i]->video != nullptr)
                    {
                        t_w = t_tracks[i]->video->i_width;
                        t_h = t_tracks[i]->video->i_height;
                        break;
                    }
                }
                if (t_tracks != nullptr)
                    libvlc_media_tracks_release(t_tracks, t_count);
            }
            *(MCRectangle *)r_value = MCRectangleMake(0, 0,
                                                      (int16_t)t_w,
                                                      (int16_t)t_h);
            break;
        }

        case kMCPlatformPlayerPropertyDuration:
        {
            // libvlc_media_player_get_length returns 0 until playback starts.
            // libvlc_media_get_duration reads the parsed container header and
            // is available immediately after libvlc_media_parse.
            MCPlatformPlayerDuration t_dur = 0;
            if (m_media != nullptr)
            {
                libvlc_time_t t_vlc = libvlc_media_get_duration(m_media);
                if (t_vlc > 0)
                    t_dur = (MCPlatformPlayerDuration)t_vlc;
            }
            if (t_dur == 0 && m_player != nullptr)
                t_dur = (MCPlatformPlayerDuration)
                            libvlc_media_player_get_length(m_player);
            *(MCPlatformPlayerDuration *)r_value = t_dur;
            break;
        }

        case kMCPlatformPlayerPropertyTimescale:
            // VLC uses milliseconds internally → timescale is 1000.
            *(MCPlatformPlayerDuration *)r_value = 1000;
            break;

        case kMCPlatformPlayerPropertyCurrentTime:
        {
            libvlc_time_t t_time = (m_player != nullptr)
                ? libvlc_media_player_get_time(m_player) : 0;
            *(MCPlatformPlayerDuration *)r_value =
                (t_time >= 0) ? (MCPlatformPlayerDuration)t_time : 0;
            break;
        }

        case kMCPlatformPlayerPropertyStartTime:
            *(MCPlatformPlayerDuration *)r_value = m_selection_start;
            break;

        case kMCPlatformPlayerPropertyFinishTime:
            *(MCPlatformPlayerDuration *)r_value = m_selection_finish;
            break;

        case kMCPlatformPlayerPropertyPlayRate:
            *(double *)r_value = m_rate;
            break;

        case kMCPlatformPlayerPropertyVolume:
            *(uint16_t *)r_value = m_volume;
            break;

        case kMCPlatformPlayerPropertyLoop:
            *(bool *)r_value = m_looping;
            break;

        case kMCPlatformPlayerPropertyOnlyPlaySelection:
            *(bool *)r_value = m_play_selection_only;
            break;

        case kMCPlatformPlayerPropertyInvalidFilename:
            *(bool *)r_value = m_has_invalid_filename;
            break;

        case kMCPlatformPlayerPropertyLoadedTime:
            *(MCPlatformPlayerDuration *)r_value = m_buffered_time;
            break;

        case kMCPlatformPlayerPropertyMediaTypes:
        {
            MCPlatformPlayerMediaTypes t_types = 0;
            if (m_media != nullptr)
            {
                libvlc_media_track_t **t_tracks = nullptr;
                unsigned t_count =
                    libvlc_media_tracks_get(m_media, &t_tracks);
                for (unsigned i = 0; i < t_count; i++)
                {
                    switch (t_tracks[i]->i_type)
                    {
                        case libvlc_track_video:
                            t_types |= kMCPlatformPlayerMediaTypeVideo; break;
                        case libvlc_track_audio:
                            t_types |= kMCPlatformPlayerMediaTypeAudio; break;
                        case libvlc_track_text:
                            t_types |= kMCPlatformPlayerMediaTypeText;  break;
                        default: break;
                    }
                }
                if (t_tracks != nullptr)
                    libvlc_media_tracks_release(t_tracks, t_count);
            }
            *(MCPlatformPlayerMediaTypes *)r_value = t_types;
            break;
        }

        // --- defaults for unsupported properties ---
        case kMCPlatformPlayerPropertyMirrored:
            *(bool *)r_value = false;
            break;
        case kMCPlatformPlayerPropertyScalefactor:
            *(double *)r_value = 1.0;
            break;
        case kMCPlatformPlayerPropertyLeftBalance:
        case kMCPlatformPlayerPropertyRightBalance:
            *(double *)r_value = 100.0;
            break;
        case kMCPlatformPlayerPropertyPan:
            *(double *)r_value = 0.0;
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Track properties
// ---------------------------------------------------------------------------

void MCVLCPlayer::CountTracks(uindex_t &r_count)
{
    r_count = 0;
    if (m_media == nullptr)
        return;

    libvlc_media_track_t **t_tracks = nullptr;
    unsigned t_count = libvlc_media_tracks_get(m_media, &t_tracks);
    r_count = (uindex_t)t_count;
    if (t_tracks != nullptr)
        libvlc_media_tracks_release(t_tracks, t_count);
}

bool MCVLCPlayer::FindTrackWithId(uint32_t p_id, uindex_t &r_index)
{
    if (m_media == nullptr)
        return false;

    libvlc_media_track_t **t_tracks = nullptr;
    unsigned t_count = libvlc_media_tracks_get(m_media, &t_tracks);

    bool t_found = false;
    for (unsigned i = 0; i < t_count; i++)
    {
        if ((uint32_t)t_tracks[i]->i_id == p_id)
        {
            r_index = (uindex_t)i;
            t_found = true;
            break;
        }
    }

    if (t_tracks != nullptr)
        libvlc_media_tracks_release(t_tracks, t_count);

    return t_found;
}

void MCVLCPlayer::SetTrackProperty(uindex_t                     p_index,
                                    MCPlatformPlayerTrackProperty p_property,
                                    MCPlatformPropertyType        p_type,
                                    void                         *p_value)
{
    // Track enable/disable is not directly supported via the VLC track API
    // in the same way — leave as no-op for now.
    (void)p_index;
    (void)p_property;
    (void)p_type;
    (void)p_value;
}

void MCVLCPlayer::GetTrackProperty(uindex_t                     p_index,
                                    MCPlatformPlayerTrackProperty p_property,
                                    MCPlatformPropertyType        p_type,
                                    void                         *r_value)
{
    if (m_media == nullptr)
        return;

    libvlc_media_track_t **t_tracks = nullptr;
    unsigned t_count = libvlc_media_tracks_get(m_media, &t_tracks);

    if (p_index >= (uindex_t)t_count)
    {
        if (t_tracks != nullptr)
            libvlc_media_tracks_release(t_tracks, t_count);
        return;
    }

    libvlc_media_track_t *t_track = t_tracks[p_index];

    switch (p_property)
    {
        case kMCPlatformPlayerTrackPropertyId:
            *(uint32_t *)r_value = (uint32_t)t_track->i_id;
            break;

        case kMCPlatformPlayerTrackPropertyMediaTypeName:
        {
            const char *t_type_name = "unknown";
            switch (t_track->i_type)
            {
                case libvlc_track_video: t_type_name = "video"; break;
                case libvlc_track_audio: t_type_name = "audio"; break;
                case libvlc_track_text:  t_type_name = "text";  break;
                default: break;
            }
            /* UNCHECKED */
            MCStringCreateWithCString(t_type_name,
                                      *(MCStringRef *)r_value);
            break;
        }

        case kMCPlatformPlayerTrackPropertyOffset:
            *(MCPlatformPlayerDuration *)r_value = 0;
            break;

        case kMCPlatformPlayerTrackPropertyDuration:
            *(MCPlatformPlayerDuration *)r_value =
                (MCPlatformPlayerDuration)libvlc_media_player_get_length(m_player);
            break;

        case kMCPlatformPlayerTrackPropertyEnabled:
            *(bool *)r_value = true;
            break;

        default:
            break;
    }

    if (t_tracks != nullptr)
        libvlc_media_tracks_release(t_tracks, t_count);
}

// ---------------------------------------------------------------------------
// VLC event callback (VLC internal thread)
// ---------------------------------------------------------------------------

void MCVLCPlayer::OnVLCEvent(const struct libvlc_event_t *p_event, void *p_opaque)
{
    MCVLCPlayer *t_self = (MCVLCPlayer *)p_opaque;

    switch (p_event->type)
    {
        case libvlc_MediaPlayerPlaying:
            // Retain so the player is not deleted before the notify fires.
            t_self->Retain();
            MCNotifyPush(DoPlaying, t_self, false, false);
            break;

        case libvlc_MediaPlayerEndReached:
            t_self->Retain();
            MCNotifyPush(DoFinished, t_self, false, false);
            break;

        case libvlc_MediaPlayerTimeChanged:
            t_self->Retain();
            MCNotifyPush(DoTimeChanged, t_self, false, false);
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Main-thread notification dispatchers
// ---------------------------------------------------------------------------

void MCVLCPlayer::DoPlaying(void *p_opaque)
{
    MCVLCPlayer *t_self = (MCVLCPlayer *)p_opaque;
    t_self->m_playing  = true;
    t_self->m_finished = false;
    t_self->Release();
}

void MCVLCPlayer::DoFinished(void *p_opaque)
{
    MCVLCPlayer *t_self = (MCVLCPlayer *)p_opaque;
    t_self->m_playing  = false;
    t_self->m_finished = true;

    if (t_self->m_looping && t_self->m_player != nullptr)
    {
        // Restart from the beginning (or selection start).
        libvlc_media_player_set_time(t_self->m_player,
            (libvlc_time_t)t_self->m_selection_start);
        libvlc_media_player_play(t_self->m_player);
    }
    else
    {
        MCPlatformCallbackSendPlayerFinished(t_self);
    }

    t_self->Release();
}

void MCVLCPlayer::DoTimeChanged(void *p_opaque)
{
    MCVLCPlayer *t_self = (MCVLCPlayer *)p_opaque;

    if (t_self->m_player == nullptr)
    {
        t_self->Release();
        return;
    }

    MCPlatformPlayerDuration t_current =
        (MCPlatformPlayerDuration)libvlc_media_player_get_time(t_self->m_player);

    // Fire any markers that have been passed.
    if (t_self->m_marker_count > 0 && t_self->m_markers != nullptr)
    {
        // Find the highest marker index whose time <= current time.
        uindex_t t_new_last = UINDEX_MAX;
        for (uindex_t i = 0; i < t_self->m_marker_count; i++)
        {
            if (t_self->m_markers[i] <= t_current)
                t_new_last = i;
        }

        if (t_new_last != UINDEX_MAX &&
            t_new_last != (uindex_t)t_self->m_last_marker)
        {
            t_self->m_last_marker = (MCPlatformPlayerDuration)t_new_last;
            MCPlatformCallbackSendPlayerMarkerChanged(
                t_self, t_self->m_markers[t_new_last]);
        }
    }

    // Enforce play-selection end boundary.
    if (t_self->m_play_selection_only &&
        t_self->m_selection_finish > t_self->m_selection_start &&
        t_current >= t_self->m_selection_finish)
    {
        libvlc_media_player_pause(t_self->m_player);
        libvlc_media_player_set_time(t_self->m_player,
            (libvlc_time_t)t_self->m_selection_finish);
        MCPlatformCallbackSendPlayerFinished(t_self);
        t_self->Release();
        return;
    }

    MCPlatformCallbackSendPlayerCurrentTimeChanged(t_self);
    t_self->Release();
}

// ---------------------------------------------------------------------------
// Offscreen video callbacks (called on VLC decode thread)
// ---------------------------------------------------------------------------

unsigned MCVLCPlayer::OnVideoFormat(void     **pp_opaque,
                                     char      *p_chroma,
                                     unsigned  *p_width,
                                     unsigned  *p_height,
                                     unsigned  *p_pitches,
                                     unsigned  *p_lines)
{
    MCVLCPlayer *t_self = (MCVLCPlayer *)*pp_opaque;

    // Request RGBA from VLC.
    memcpy(p_chroma, "RV32", 4);

    // Remember the negotiated size.
    t_self->m_offscreen_width  = *p_width;
    t_self->m_offscreen_height = *p_height;

    // Bytes per row (4 bytes per pixel, aligned to 4 bytes).
    p_pitches[0] = ((*p_width) * 4 + 3) & ~3u;
    p_lines[0]   = *p_height;

    // Allocate the backing bitmap.
    if (t_self->m_offscreen_bitmap != nullptr)
        MCImageFreeBitmap(t_self->m_offscreen_bitmap);

    MCImageBitmapCreate(*p_width, *p_height, t_self->m_offscreen_bitmap);
    if (t_self->m_offscreen_bitmap != nullptr)
        t_self->m_offscreen_planes[0] = t_self->m_offscreen_bitmap->data;
    else
        t_self->m_offscreen_planes[0] = nullptr;

    return 1; // one plane
}

void MCVLCPlayer::OnVideoCleanup(void *p_opaque)
{
    MCVLCPlayer *t_self = (MCVLCPlayer *)p_opaque;
    if (t_self->m_offscreen_bitmap != nullptr)
    {
        MCImageFreeBitmap(t_self->m_offscreen_bitmap);
        t_self->m_offscreen_bitmap     = nullptr;
        t_self->m_offscreen_planes[0]  = nullptr;
    }
}

void *MCVLCPlayer::OnVideoLock(void *p_opaque, void **pp_planes)
{
    MCVLCPlayer *t_self = (MCVLCPlayer *)p_opaque;
    pp_planes[0] = t_self->m_offscreen_planes[0];
    return nullptr; // no picture token needed
}

void MCVLCPlayer::OnVideoUnlock(void *p_opaque, void *p_picture,
                                  void *const *pp_planes)
{
    // Nothing to do — the bitmap is written in place.
    (void)p_opaque;
    (void)p_picture;
    (void)pp_planes;
}

static void MCVLCDoFrameChanged(void *p_opaque)
{
    MCVLCPlayer *t_self = (MCVLCPlayer *)p_opaque;
    MCPlatformCallbackSendPlayerFrameChanged(t_self);
    t_self->Release();
}

void MCVLCPlayer::OnVideoDisplay(void *p_opaque, void *p_picture)
{
    MCVLCPlayer *t_self = (MCVLCPlayer *)p_opaque;
    // Notify the engine on the main thread that a new frame is ready.
    t_self->Retain();
    MCNotifyPush(MCVLCDoFrameChanged, t_self, false, false);
    (void)p_picture;
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

MCVLCPlayer *MCVLCPlayerCreate()
{
    return new (nothrow) MCVLCPlayer();
}
