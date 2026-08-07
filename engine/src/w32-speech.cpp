/*
Copyright (C) 2026 HyperXTalk

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

HyperXTalk is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
HyperXTalk. If not, see <http://www.gnu.org/licenses/>.
*/

//
// Windows SAPI 5 voice command backend.
//
// Architecture overview
// ─────────────────────
// A dedicated STA worker thread owns all SAPI / COM objects and runs a Win32
// message loop.  SAPI delivers recognition events as WM_SPH_SAPI messages to
// a hidden HWND_MESSAGE window (s_worker_hwnd) that lives on that thread.
//
// A second hidden HWND_MESSAGE window (s_notify_hwnd) lives on the engine
// main thread.  The worker posts WM_SPH_* messages there so that every
// MCSpeechDispatch* call lands on the main thread, matching the Mac backend's
// behaviour of marshalling to the main run loop.
//
// Shared state (phrase list, wake word, is-listening flag, HWNDs) is
// protected by s_cs (a CRITICAL_SECTION).  Worker-local SAPI objects are
// never touched from the main thread once the worker has started.
//
// Grammar / wake-word modes
// ─────────────────────────
// When a wake word is set and no command window is open, only the "WakeWord"
// SAPI rule is active.  On a wake-word match, wakeWordDetected is dispatched,
// the "Commands" rule is activated, and a timer is started for the configured
// command-window duration.  If a command matches the timer is cancelled and
// the grammar returns to wake-word-only mode.  If the timer fires first,
// listenTimeoutExpired is dispatched and the grammar returns to wake-word-only
// mode.
//
// When no wake word is set ("always-on"), the "Commands" rule is active at all
// times and every match fires voiceCommand immediately.
//
// Unrecognised speech (SPEI_FALSE_RECOGNITION) dispatches unrecognizedVoiceCommand
// only when in always-on mode or inside an active command window — never while
// waiting for the wake word.
//

#include "prefix.h"

// initguid.h causes the Windows SDK to emit inline definitions for all GUIDs
// that follow it (CLSID_SpSharedRecognizer, IID_ISpRecognizer, etc.), so we
// do not need to link sapi.lib just for the GUID constants.
#include <initguid.h>
#include <sapi.h>
#include <string>
#include <vector>

#include "mcstring.h"
#include "globals.h"
#include "variable.h"   // MCVariable full definition — needed for MCresult->setvalueref()
#include "speech.h"

// ── Custom window messages ────────────────────────────────────────────────────

// Worker → main thread  (PostMessage to s_notify_hwnd)
#define WM_SPH_VOICE_COMMAND  (WM_APP + 100) // lParam = MCStringRef (main releases)
#define WM_SPH_WAKE_WORD      (WM_APP + 101)
#define WM_SPH_TIMEOUT        (WM_APP + 102)
#define WM_SPH_UNRECOGNIZED   (WM_APP + 103) // lParam = MCStringRef (main releases)
#define WM_SPH_STARTED        (WM_APP + 104)
#define WM_SPH_FAILED         (WM_APP + 105) // lParam = MCStringRef (main releases)

// Main → worker thread  (PostMessage to s_worker_hwnd)
#define WM_SPH_SAPI           (WM_APP + 200) // SAPI notification relay
#define WM_SPH_UPDATE_GRAMMAR (WM_APP + 201)
#define WM_SPH_STOP           (WM_APP + 202)

// Timer ID used on the worker thread for the post-wake-word command window
#define TIMER_CMD_WINDOW 1

// ── Shared state  (every field protected by s_cs) ────────────────────────────

static CRITICAL_SECTION           s_cs;
static bool                       s_cs_init         = false;

static std::vector<std::wstring>  s_phrases;          // current registered phrases
static std::wstring               s_wake_word;        // empty = no wake word / always-on
static uint32_t                   s_wake_timeout_ms  = 5000;
static bool                       s_is_listening     = false;

static HWND                       s_notify_hwnd      = NULL; // main-thread dispatcher
static HWND                       s_worker_hwnd      = NULL; // worker SAPI HWND
static HANDLE                     s_hwnd_ready       = NULL; // event: worker HWND is ready
static HANDLE                     s_worker_thread_h  = NULL;

// ── Worker-thread-local  (never read/written from main after worker starts) ──

static ISpRecognizer             *s_recognizer       = nullptr;
static ISpRecoContext            *s_context          = nullptr;
static ISpRecoGrammar            *s_grammar          = nullptr;
static bool                       s_cmd_window_open  = false;

// ── Forward declarations ──────────────────────────────────────────────────────

static LRESULT CALLBACK _notify_wnd_proc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK _worker_wnd_proc(HWND, UINT, WPARAM, LPARAM);
static DWORD   WINAPI   _worker_thread(LPVOID);
static void             _worker_rebuild_grammar();
static void             _worker_handle_sapi_events();
static bool             _ensure_notify_hwnd();

// ── Helpers ───────────────────────────────────────────────────────────────────

static void _ensure_cs()
{
    if (!s_cs_init)
    {
        InitializeCriticalSection(&s_cs);
        s_cs_init = true;
    }
}

// Post a retained MCStringRef to s_notify_hwnd.
// Transfers ownership to the message recipient, which must release it.
// Safe to call from the worker thread — s_notify_hwnd is written once on the
// main thread before CreateThread (happens-before edge), so no lock needed.
static void _post_string_to_main(UINT p_msg, MCStringRef p_str)
{
    MCStringRef t_copy = MCValueRetain(p_str);
    if (!PostMessage(s_notify_hwnd, p_msg, 0, (LPARAM)t_copy))
        MCValueRelease(t_copy); // queue full or window gone
}

// ── Notify HWND window procedure  (main thread) ───────────────────────────────

static LRESULT CALLBACK _notify_wnd_proc(HWND p_hwnd, UINT p_msg,
                                          WPARAM /*wp*/, LPARAM lp)
{
    switch (p_msg)
    {
        case WM_SPH_VOICE_COMMAND:
        {
            MCStringRef t_phrase = reinterpret_cast<MCStringRef>(lp);
            MCSpeechDispatchVoiceCommand(t_phrase);
            MCValueRelease(t_phrase);
            return 0;
        }
        case WM_SPH_WAKE_WORD:
            MCSpeechDispatchWakeWordDetected();
            return 0;

        case WM_SPH_TIMEOUT:
            MCSpeechDispatchListenTimeoutExpired();
            return 0;

        case WM_SPH_UNRECOGNIZED:
        {
            MCStringRef t_text = reinterpret_cast<MCStringRef>(lp);
            MCSpeechDispatchUnrecognizedInput(t_text);
            MCValueRelease(t_text);
            return 0;
        }
        case WM_SPH_STARTED:
            MCSpeechDispatchListeningStarted();
            return 0;

        case WM_SPH_FAILED:
        {
            MCStringRef t_reason = reinterpret_cast<MCStringRef>(lp);
            MCSpeechDispatchListeningFailed(t_reason);
            MCValueRelease(t_reason);
            return 0;
        }
        default:
            break;
    }
    return DefWindowProc(p_hwnd, p_msg, 0, lp);
}

// ── Grammar management  (worker thread only) ──────────────────────────────────

// Rebuilds the SAPI CFG grammar to match the current phrase list and wake
// word.  Takes s_cs briefly to snapshot the shared lists, then releases before
// touching any COM object.
static void _worker_rebuild_grammar()
{
    if (!s_grammar)
        return;

    // Snapshot shared state.
    std::vector<std::wstring> t_phrases;
    std::wstring              t_wake_word;
    {
        EnterCriticalSection(&s_cs);
        t_phrases   = s_phrases;
        t_wake_word = s_wake_word;
        LeaveCriticalSection(&s_cs);
    }

    bool t_have_phrases   = !t_phrases.empty();
    bool t_have_wake_word = !t_wake_word.empty();

    // Clear all existing rules.
    s_grammar->ResetGrammar(MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT));

    // Build "Commands" rule.
    if (t_have_phrases)
    {
        SPSTATEHANDLE t_state = 0;
        if (SUCCEEDED(s_grammar->GetRule(L"Commands", 0,
                                          SPRAF_TopLevel | SPRAF_Active,
                                          TRUE, &t_state)))
        {
            for (const std::wstring& t_phrase : t_phrases)
            {
                s_grammar->AddWordTransition(t_state, NULL,
                                              t_phrase.c_str(), L" ",
                                              SPWT_LEXICAL, 1.0f, NULL);
            }
        }
    }

    // Build "WakeWord" rule.
    if (t_have_wake_word)
    {
        SPSTATEHANDLE t_ww_state = 0;
        if (SUCCEEDED(s_grammar->GetRule(L"WakeWord", 0,
                                          SPRAF_TopLevel | SPRAF_Active,
                                          TRUE, &t_ww_state)))
        {
            s_grammar->AddWordTransition(t_ww_state, NULL,
                                          t_wake_word.c_str(), L" ",
                                          SPWT_LEXICAL, 1.0f, NULL);
        }
    }

    s_grammar->Commit(0);

    // Activate the right rule(s) for the current mode.
    if (t_have_wake_word && !s_cmd_window_open)
    {
        // Wake-word-only: WakeWord active, Commands inactive.
        s_grammar->SetRuleState(L"WakeWord", NULL, SPRS_ACTIVE);
        if (t_have_phrases)
            s_grammar->SetRuleState(L"Commands", NULL, SPRS_INACTIVE);
    }
    else
    {
        // Always-on or inside command window: Commands active, WakeWord inactive.
        if (t_have_phrases)
            s_grammar->SetRuleState(L"Commands", NULL, SPRS_ACTIVE);
        if (t_have_wake_word)
            s_grammar->SetRuleState(L"WakeWord", NULL, SPRS_INACTIVE);
    }
}

// ── SAPI event handler  (worker thread only) ──────────────────────────────────

static void _worker_handle_sapi_events()
{
    if (!s_context)
        return;

    SPEVENT t_event   = {};
    ULONG   t_fetched = 0;

    while (s_context->GetEvents(1, &t_event, &t_fetched) == S_OK && t_fetched > 0)
    {
        if (t_event.eEventId == SPEI_RECOGNITION)
        {
            ISpRecoResult *t_result = reinterpret_cast<ISpRecoResult *>(t_event.lParam);
            if (!t_result)
                continue;

            // Determine whether this is a wake-word or command recognition.
            SPPHRASE *t_phrase_info = nullptr;
            t_result->GetPhrase(&t_phrase_info);

            bool t_is_wake_word = false;
            if (t_phrase_info && t_phrase_info->Rule.pszName)
                t_is_wake_word = (wcscmp(t_phrase_info->Rule.pszName, L"WakeWord") == 0);
            if (t_phrase_info)
                CoTaskMemFree(t_phrase_info);

            // Get the recognised transcript.
            WCHAR *t_text = nullptr;
            t_result->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE,
                               FALSE, &t_text, NULL);

            if (t_is_wake_word)
            {
                // Notify the main thread.
                PostMessage(s_notify_hwnd, WM_SPH_WAKE_WORD, 0, 0);

                // Open the command window.
                s_cmd_window_open = true;
                _worker_rebuild_grammar();

                // Start the timeout timer.
                uint32_t t_ms;
                EnterCriticalSection(&s_cs);
                t_ms = s_wake_timeout_ms;
                LeaveCriticalSection(&s_cs);
                SetTimer(s_worker_hwnd, TIMER_CMD_WINDOW, t_ms, NULL);
            }
            else
            {
                // Command match — cancel any running timer and close the window.
                KillTimer(s_worker_hwnd, TIMER_CMD_WINDOW);
                s_cmd_window_open = false;

                // Build an MCStringRef from the transcript and post to main.
                if (t_text && t_text[0] != L'\0')
                {
                    MCStringRef t_str = nil;
                    /* UNCHECKED */ MCStringCreateWithChars(
                        reinterpret_cast<const unichar_t *>(t_text),
                        static_cast<uindex_t>(wcslen(t_text)),
                        t_str);
                    if (t_str)
                    {
                        _post_string_to_main(WM_SPH_VOICE_COMMAND, t_str);
                        MCValueRelease(t_str);
                    }
                }

                // Return grammar to the appropriate mode.
                _worker_rebuild_grammar();
            }

            if (t_text)
                CoTaskMemFree(t_text);
            t_result->Release();
        }
        else if (t_event.eEventId == SPEI_FALSE_RECOGNITION)
        {
            // Dispatch unrecognizedVoiceCommand only in always-on mode or
            // during an active command window — never while waiting for the wake word.
            bool t_have_wake_word;
            {
                EnterCriticalSection(&s_cs);
                t_have_wake_word = !s_wake_word.empty();
                LeaveCriticalSection(&s_cs);
            }

            bool t_dispatch = s_cmd_window_open || !t_have_wake_word;

            ISpRecoResult *t_result = reinterpret_cast<ISpRecoResult *>(t_event.lParam);
            if (t_result)
            {
                if (t_dispatch)
                {
                    WCHAR *t_text = nullptr;
                    t_result->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE,
                                       FALSE, &t_text, NULL);
                    if (t_text && t_text[0] != L'\0')
                    {
                        MCStringRef t_str = nil;
                        /* UNCHECKED */ MCStringCreateWithChars(
                            reinterpret_cast<const unichar_t *>(t_text),
                            static_cast<uindex_t>(wcslen(t_text)),
                            t_str);
                        if (t_str)
                        {
                            _post_string_to_main(WM_SPH_UNRECOGNIZED, t_str);
                            MCValueRelease(t_str);
                        }
                    }
                    if (t_text)
                        CoTaskMemFree(t_text);
                }
                t_result->Release();
            }
        }
        else if (t_event.elParamType == SPET_LPARAM_IS_OBJECT && t_event.lParam)
        {
            // Release any other COM object we received but didn't handle.
            reinterpret_cast<IUnknown *>(t_event.lParam)->Release();
        }
    }
}

// ── Worker HWND window procedure  (worker thread) ────────────────────────────

static LRESULT CALLBACK _worker_wnd_proc(HWND p_hwnd, UINT p_msg,
                                          WPARAM wp, LPARAM lp)
{
    switch (p_msg)
    {
        case WM_SPH_SAPI:
            _worker_handle_sapi_events();
            return 0;

        case WM_SPH_UPDATE_GRAMMAR:
            _worker_rebuild_grammar();
            return 0;

        case WM_SPH_STOP:
            // Release SAPI objects, then quit the worker message loop.
            KillTimer(p_hwnd, TIMER_CMD_WINDOW);
            s_cmd_window_open = false;

            if (s_grammar)    { s_grammar->Release();    s_grammar    = nullptr; }
            if (s_context)    { s_context->Release();    s_context    = nullptr; }
            if (s_recognizer) { s_recognizer->Release(); s_recognizer = nullptr; }

            PostQuitMessage(0);
            return 0;

        case WM_TIMER:
            if (static_cast<UINT_PTR>(wp) == TIMER_CMD_WINDOW)
            {
                KillTimer(p_hwnd, TIMER_CMD_WINDOW);
                s_cmd_window_open = false;
                _worker_rebuild_grammar();
                PostMessage(s_notify_hwnd, WM_SPH_TIMEOUT, 0, 0);
                return 0;
            }
            break;

        default:
            break;
    }
    return DefWindowProc(p_hwnd, p_msg, wp, lp);
}

// ── Worker thread entry point ─────────────────────────────────────────────────

static DWORD WINAPI _worker_thread(LPVOID /*unused*/)
{
    // SAPI COM objects require an STA.
    CoInitialize(NULL);

    // Create the hidden message-only HWND for this worker thread.
    static const wchar_t k_worker_class[] = L"HXTSpeechWorker";
    {
        WNDCLASSW t_wc       = {};
        t_wc.lpfnWndProc     = _worker_wnd_proc;
        t_wc.hInstance       = GetModuleHandleW(NULL);
        t_wc.lpszClassName   = k_worker_class;
        RegisterClassW(&t_wc); // safe to call more than once; duplicate is ignored
    }

    HWND t_hwnd = CreateWindowExW(0, k_worker_class, L"", 0,
                                   0, 0, 0, 0,
                                   HWND_MESSAGE, NULL,
                                   GetModuleHandleW(NULL), NULL);
    if (!t_hwnd)
    {
        MCStringRef t_err = nil;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: failed to create SAPI worker window", t_err);
        _post_string_to_main(WM_SPH_FAILED, t_err);
        MCValueRelease(t_err);

        EnterCriticalSection(&s_cs);
        s_is_listening = false;
        LeaveCriticalSection(&s_cs);

        SetEvent(s_hwnd_ready);
        CoUninitialize();
        return 1;
    }

    // Publish the worker HWND, then signal the main thread that its message
    // queue is ready.  The main thread may return from startListening as soon
    // as the event is set; SAPI setup continues asynchronously.
    EnterCriticalSection(&s_cs);
    s_worker_hwnd = t_hwnd;
    LeaveCriticalSection(&s_cs);

    SetEvent(s_hwnd_ready);

    // ── SAPI setup ────────────────────────────────────────────────────────────

    HRESULT t_hr = CoCreateInstance(CLSID_SpSharedRecognizer, NULL,
                                     CLSCTX_INPROC_SERVER,
                                     IID_ISpRecognizer,
                                     reinterpret_cast<void **>(&s_recognizer));
    if (FAILED(t_hr))
    {
        MCStringRef t_err = nil;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: CoCreateInstance(SpSharedRecognizer) failed", t_err);
        _post_string_to_main(WM_SPH_FAILED, t_err);
        MCValueRelease(t_err);
        goto done;
    }

    t_hr = s_recognizer->CreateRecoContext(&s_context);
    if (FAILED(t_hr))
    {
        MCStringRef t_err = nil;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: CreateRecoContext failed", t_err);
        _post_string_to_main(WM_SPH_FAILED, t_err);
        MCValueRelease(t_err);
        goto done;
    }

    {
        // Subscribe to recognition and false-recognition events only.
        ULONGLONG t_interest = SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_FALSE_RECOGNITION);
        t_hr = s_context->SetInterest(t_interest, t_interest);
        if (FAILED(t_hr))
        {
            MCStringRef t_err = nil;
            /* UNCHECKED */ MCStringCreateWithCString(
                "startListening: SetInterest failed", t_err);
            _post_string_to_main(WM_SPH_FAILED, t_err);
            MCValueRelease(t_err);
            goto done;
        }
    }

    // Deliver SAPI notifications as WM_SPH_SAPI to the worker window.
    t_hr = s_context->SetNotifyWindowMessage(t_hwnd, WM_SPH_SAPI, 0, 0);
    if (FAILED(t_hr))
    {
        MCStringRef t_err = nil;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: SetNotifyWindowMessage failed", t_err);
        _post_string_to_main(WM_SPH_FAILED, t_err);
        MCValueRelease(t_err);
        goto done;
    }

    t_hr = s_context->CreateGrammar(0, &s_grammar);
    if (FAILED(t_hr))
    {
        MCStringRef t_err = nil;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: CreateGrammar failed", t_err);
        _post_string_to_main(WM_SPH_FAILED, t_err);
        MCValueRelease(t_err);
        goto done;
    }

    // Populate grammar from any phrases that are already registered.
    _worker_rebuild_grammar();

    // All ready — notify the main thread asynchronously.
    PostMessage(s_notify_hwnd, WM_SPH_STARTED, 0, 0);

    // ── Message loop ──────────────────────────────────────────────────────────
    {
        MSG t_msg;
        while (GetMessage(&t_msg, NULL, 0, 0) > 0)
        {
            TranslateMessage(&t_msg);
            DispatchMessage(&t_msg);
        }
    }

done:
    // Release any SAPI objects still held (WM_SPH_STOP releases them too, but
    // we guard against double-release with nullptr checks).
    if (s_grammar)    { s_grammar->Release();    s_grammar    = nullptr; }
    if (s_context)    { s_context->Release();    s_context    = nullptr; }
    if (s_recognizer) { s_recognizer->Release(); s_recognizer = nullptr; }

    DestroyWindow(t_hwnd);

    EnterCriticalSection(&s_cs);
    s_worker_hwnd  = NULL;
    s_is_listening = false;
    LeaveCriticalSection(&s_cs);

    CoUninitialize();
    return 0;
}

// ── Notify HWND bootstrap  (called once, on the main thread) ─────────────────

static bool _ensure_notify_hwnd()
{
    if (s_notify_hwnd != NULL)
        return true;

    static const wchar_t k_notify_class[] = L"HXTSpeechNotify";
    {
        WNDCLASSW t_wc     = {};
        t_wc.lpfnWndProc   = _notify_wnd_proc;
        t_wc.hInstance     = GetModuleHandleW(NULL);
        t_wc.lpszClassName = k_notify_class;
        RegisterClassW(&t_wc);
    }

    s_notify_hwnd = CreateWindowExW(0, k_notify_class, L"", 0,
                                     0, 0, 0, 0,
                                     HWND_MESSAGE, NULL,
                                     GetModuleHandleW(NULL), NULL);
    return (s_notify_hwnd != NULL);
}

// ── Platform entry points ─────────────────────────────────────────────────────

bool MCPlatformStartListening(MCStringRef p_language)
{
    _ensure_cs();

    {
        EnterCriticalSection(&s_cs);
        bool t_already = s_is_listening;
        LeaveCriticalSection(&s_cs);
        if (t_already)
            return true; // already running — idempotent
    }

    if (!_ensure_notify_hwnd())
    {
        MCStringRef t_err = nil;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: failed to create notify window", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    // p_language (BCP-47 tag) is accepted for API compatibility.  SAPI uses the
    // shared recognizer's current audio device locale; locale switching via
    // ISpObjectToken is left for a future implementation pass.
    (void)p_language;

    // An event lets us wait until the worker's message queue is ready before
    // returning, so callers can immediately post commands to it.
    s_hwnd_ready = CreateEventW(NULL, TRUE /*manual-reset*/, FALSE, NULL);
    if (!s_hwnd_ready)
    {
        MCStringRef t_err = nil;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: CreateEvent failed", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    EnterCriticalSection(&s_cs);
    s_is_listening = true;
    LeaveCriticalSection(&s_cs);

    s_worker_thread_h = CreateThread(NULL, 0, _worker_thread, NULL, 0, NULL);
    if (!s_worker_thread_h)
    {
        CloseHandle(s_hwnd_ready);
        s_hwnd_ready = NULL;

        EnterCriticalSection(&s_cs);
        s_is_listening = false;
        LeaveCriticalSection(&s_cs);

        MCStringRef t_err = nil;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: CreateThread failed", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    // Wait up to 2 s for the worker to publish its HWND.  This makes the
    // synchronous startListening return feel instant (grammar updates can be
    // posted as soon as we return).  listeningStarted or listeningFailed will
    // arrive asynchronously via the notify HWND once SAPI setup completes.
    WaitForSingleObject(s_hwnd_ready, 2000);
    CloseHandle(s_hwnd_ready);
    s_hwnd_ready = NULL;

    // Return true: success/failure is reported via the async dispatch messages.
    return true;
}

void MCPlatformStopListening()
{
    _ensure_cs();

    HWND   t_worker;
    HANDLE t_thread;
    {
        EnterCriticalSection(&s_cs);
        t_worker          = s_worker_hwnd;
        t_thread          = s_worker_thread_h;
        s_is_listening    = false;
        s_worker_thread_h = NULL; // transfer ownership to this scope
        LeaveCriticalSection(&s_cs);
    }

    if (t_worker != NULL)
        PostMessage(t_worker, WM_SPH_STOP, 0, 0);

    if (t_thread != NULL)
    {
        WaitForSingleObject(t_thread, 3000);
        CloseHandle(t_thread);
    }
}

bool MCPlatformIsListening()
{
    _ensure_cs();
    EnterCriticalSection(&s_cs);
    bool t_val = s_is_listening;
    LeaveCriticalSection(&s_cs);
    return t_val;
}

void MCPlatformSetVoiceCommands(MCStringRef p_phrases)
{
    _ensure_cs();

    // Convert the return-delimited MCStringRef into a vector of wide strings
    // by iterating its UTF-16 code units directly — avoids any intermediate
    // encoding conversion.
    std::vector<std::wstring> t_new_phrases;

    if (!MCStringIsEmpty(p_phrases))
    {
        uindex_t t_len   = MCStringGetLength(p_phrases);
        uindex_t t_start = 0;

        for (uindex_t i = 0; i <= t_len; i++)
        {
            bool t_boundary = (i == t_len) ||
                              (MCStringGetCharAtIndex(p_phrases, i) == '\n');
            if (!t_boundary)
                continue;

            if (i > t_start)
            {
                std::wstring t_phrase;
                t_phrase.reserve(i - t_start);
                for (uindex_t j = t_start; j < i; j++)
                    t_phrase += static_cast<wchar_t>(
                                    MCStringGetCharAtIndex(p_phrases, j));
                t_new_phrases.push_back(std::move(t_phrase));
            }
            t_start = i + 1;
        }
    }

    HWND t_worker;
    {
        EnterCriticalSection(&s_cs);
        s_phrases = std::move(t_new_phrases);
        t_worker  = s_worker_hwnd;
        LeaveCriticalSection(&s_cs);
    }

    if (t_worker != NULL)
        PostMessage(t_worker, WM_SPH_UPDATE_GRAMMAR, 0, 0);
}

void MCPlatformSetWakeWord(MCStringRef p_word, uint32_t p_timeout_ms)
{
    _ensure_cs();

    std::wstring t_new_word;
    if (p_word != nil && !MCStringIsEmpty(p_word))
    {
        uindex_t t_len = MCStringGetLength(p_word);
        t_new_word.reserve(t_len);
        for (uindex_t i = 0; i < t_len; i++)
            t_new_word += static_cast<wchar_t>(MCStringGetCharAtIndex(p_word, i));
    }

    HWND t_worker;
    {
        EnterCriticalSection(&s_cs);
        s_wake_word       = std::move(t_new_word);
        s_wake_timeout_ms = (p_timeout_ms > 0) ? p_timeout_ms : 5000;
        t_worker          = s_worker_hwnd;
        LeaveCriticalSection(&s_cs);
    }

    if (t_worker != NULL)
        PostMessage(t_worker, WM_SPH_UPDATE_GRAMMAR, 0, 0);
}
