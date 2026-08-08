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
// Linux voice command backend — Vosk + PulseAudio.
//
// Architecture
// ────────────
// A dedicated POSIX thread owns audio capture (PulseAudio simple API) and
// speech recognition (Vosk).  The thread reads 16-bit mono PCM at 16 kHz in
// 4096-sample chunks, feeds them to a VoskRecognizer, and processes results.
//
// When Vosk signals a final result (vosk_recognizer_accept_waveform returns 1)
// the JSON transcript is extracted and matched against the registered phrase
// list.  Callbacks are delivered to the HXT main thread via g_idle_add so they
// land on the GLib event loop — the same pattern used by GTK signal handlers.
//
// Both libvosk and libpulse-simple are loaded at runtime via dlopen so that
// HyperXTalk can launch on systems without these libraries installed; the
// feature is simply unavailable in that case.
//
// Wake word mode
// ──────────────
// When a wake word is set the recognizer grammar is limited to just the wake
// word.  On detection, wakeWordDetected is dispatched, the grammar is rebuilt
// to include all registered commands, and a command-window timer fires via
// GLib (g_timeout_add) if no command is matched within the configured window.
//
// Model location
// ──────────────
// Vosk requires a model directory.  Locations tried in order:
//   1. $VOSK_MODEL_PATH environment variable
//   2. ~/.local/share/vosk/model
//   3. /usr/share/vosk/model
//   4. /usr/local/share/vosk/model
//   5. /opt/vosk/model
//
// A small English model (~50 MB) is available from https://alphacephei.com/vosk/models
//

#include "lnxprefix.h"
#include "mcstring.h"
#include "globals.h"
#include "variable.h"   // MCVariable full definition — needed for MCresult->setvalueref()
#include "speech.h"

#include <pthread.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <glib.h>   // g_idle_add, g_timeout_add

// G_SOURCE_REMOVE / G_SOURCE_CONTINUE were added in GLib 2.32.
// Define fallbacks for older build environments.
#ifndef G_SOURCE_REMOVE
#  define G_SOURCE_REMOVE  FALSE
#endif
#ifndef G_SOURCE_CONTINUE
#  define G_SOURCE_CONTINUE TRUE
#endif

////////////////////////////////////////////////////////////////////////////////
// PulseAudio simple API — types and function pointers (loaded via dlopen)

typedef struct pa_simple pa_simple;

struct pa_sample_spec {
    uint8_t  format;      // PA_SAMPLE_S16LE = 3
    uint32_t rate;
    uint8_t  channels;
};

enum { PA_STREAM_RECORD = 2 };
enum { PA_SAMPLE_S16LE  = 3 };

typedef pa_simple* (*pa_simple_new_fn)(
    const char *server, const char *name, int dir,
    const char *dev, const char *stream_name,
    const pa_sample_spec *ss, const void *map,
    const void *attr, int *error);
typedef void (*pa_simple_free_fn)(pa_simple *s);
typedef int  (*pa_simple_read_fn)(pa_simple *s, void *data, size_t bytes, int *error);

static void             *s_pa_lib   = nullptr;
static pa_simple_new_fn  s_pa_new   = nullptr;
static pa_simple_free_fn s_pa_free  = nullptr;
static pa_simple_read_fn s_pa_read  = nullptr;

////////////////////////////////////////////////////////////////////////////////
// Vosk API — types and function pointers (loaded via dlopen)

typedef struct VoskModel      VoskModel;
typedef struct VoskRecognizer VoskRecognizer;

typedef void            (*vosk_set_log_level_fn)(int level);
typedef VoskModel*      (*vosk_model_new_fn)(const char *model_path);
typedef void            (*vosk_model_free_fn)(VoskModel *model);
typedef VoskRecognizer* (*vosk_recognizer_new_grm_fn)(VoskModel *model, float sample_rate, const char *grammar);
typedef void            (*vosk_recognizer_free_fn)(VoskRecognizer *recognizer);
typedef int             (*vosk_recognizer_accept_waveform_fn)(VoskRecognizer *recognizer, const char *data, int length);
typedef const char*     (*vosk_recognizer_result_fn)(VoskRecognizer *recognizer);
typedef const char*     (*vosk_recognizer_final_result_fn)(VoskRecognizer *recognizer);

static void                           *s_vosk_lib        = nullptr;
static vosk_set_log_level_fn           s_vosk_log        = nullptr;
static vosk_model_new_fn               s_vosk_model_new  = nullptr;
static vosk_model_free_fn              s_vosk_model_free = nullptr;
static vosk_recognizer_new_grm_fn      s_vosk_rec_new    = nullptr;
static vosk_recognizer_free_fn         s_vosk_rec_free   = nullptr;
static vosk_recognizer_accept_waveform_fn s_vosk_accept  = nullptr;
static vosk_recognizer_result_fn       s_vosk_result     = nullptr;
static vosk_recognizer_final_result_fn s_vosk_final      = nullptr;

////////////////////////////////////////////////////////////////////////////////
// Shared state — protected by s_mutex

static pthread_mutex_t  s_mutex          = PTHREAD_MUTEX_INITIALIZER;
static pthread_t        s_thread;
static bool             s_thread_running = false;
static volatile bool    s_stop_requested = false;
static bool             s_is_listening   = false;

// Phrase list kept in sync with the cross-platform registry.
static std::vector<std::string> s_phrases;

// Wake word state.
static std::string  s_wake_word;
static uint32_t     s_wake_timeout_ms  = 5000;
static bool         s_window_open      = false; // command window active
static guint        s_timeout_src_id   = 0;     // GLib timer source id

// Vosk model (loaded once, reused across start/stop cycles).
static VoskModel   *s_model            = nullptr;

////////////////////////////////////////////////////////////////////////////////
// Library loading

static bool _load_pulse()
{
    if (s_pa_lib) return true;

    const char *k_names[] = { "libpulse-simple.so.0", "libpulse-simple.so", nullptr };
    for (const char **p = k_names; *p; p++)
    {
        s_pa_lib = dlopen(*p, RTLD_LAZY | RTLD_GLOBAL);
        if (s_pa_lib) break;
    }
    if (!s_pa_lib) return false;

    s_pa_new  = (pa_simple_new_fn) dlsym(s_pa_lib, "pa_simple_new");
    s_pa_free = (pa_simple_free_fn)dlsym(s_pa_lib, "pa_simple_free");
    s_pa_read = (pa_simple_read_fn)dlsym(s_pa_lib, "pa_simple_read");
    return s_pa_new && s_pa_free && s_pa_read;
}

static bool _load_vosk()
{
    if (s_vosk_lib) return true;

    const char *k_names[] = { "libvosk.so", "libvosk.so.0", nullptr };
    for (const char **p = k_names; *p; p++)
    {
        s_vosk_lib = dlopen(*p, RTLD_LAZY | RTLD_GLOBAL);
        if (s_vosk_lib) break;
    }
    if (!s_vosk_lib) return false;

#define LOAD(var, sym) \
    var = (decltype(var))dlsym(s_vosk_lib, sym); \
    if (!var) return false;

    LOAD(s_vosk_log,       "vosk_set_log_level")
    LOAD(s_vosk_model_new, "vosk_model_new")
    LOAD(s_vosk_model_free,"vosk_model_free")
    LOAD(s_vosk_rec_new,   "vosk_recognizer_new_grm")
    LOAD(s_vosk_rec_free,  "vosk_recognizer_free")
    LOAD(s_vosk_accept,    "vosk_recognizer_accept_waveform")
    LOAD(s_vosk_result,    "vosk_recognizer_result")
    LOAD(s_vosk_final,     "vosk_recognizer_final_result")
#undef LOAD
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Model discovery

static std::string _find_model_path()
{
    const char *t_env = getenv("VOSK_MODEL_PATH");
    if (t_env && t_env[0] != '\0')
    {
        struct stat t_st;
        if (stat(t_env, &t_st) == 0 && S_ISDIR(t_st.st_mode))
            return t_env;
    }

    std::vector<std::string> t_candidates;
    const char *t_home = getenv("HOME");
    if (t_home)
        t_candidates.push_back(std::string(t_home) + "/.local/share/vosk/model");
    t_candidates.push_back("/usr/share/vosk/model");
    t_candidates.push_back("/usr/local/share/vosk/model");
    t_candidates.push_back("/opt/vosk/model");

    for (const auto& t_path : t_candidates)
    {
        struct stat t_st;
        if (stat(t_path.c_str(), &t_st) == 0 && S_ISDIR(t_st.st_mode))
            return t_path;
    }
    return {};
}

////////////////////////////////////////////////////////////////////////////////
// Grammar builder
//
// Vosk grammar is a JSON array of accepted phrases.  We always include "[unk]"
// so unrecognised speech surfaces as a result rather than being silently dropped
// — this lets us dispatch unrecognizedVoiceCommand instead of nothing.

static std::string _build_grammar(const std::vector<std::string>& p_phrases,
                                  const std::string& p_extra = {})
{
    std::string t_json = "[";
    bool t_first = true;

    auto _append = [&](const std::string& s) {
        if (!t_first) t_json += ", ";
        t_first = false;
        t_json += "\"";
        for (char c : s)
        {
            if (c == '"')       t_json += "\\\"";
            else if (c == '\\') t_json += "\\\\";
            else                t_json += c;
        }
        t_json += "\"";
    };

    for (const auto& t_p : p_phrases) _append(t_p);
    if (!p_extra.empty()) _append(p_extra);
    _append("[unk]");

    t_json += "]";
    return t_json;
}

////////////////////////////////////////////////////////////////////////////////
// JSON transcript extraction
//
// Vosk returns e.g.:  { "text" : "open stack" }
// We do a minimal scan — no full JSON parser needed for this simple format.

static std::string _extract_text(const char *p_json)
{
    if (!p_json) return {};

    const char *t_key = strstr(p_json, "\"text\"");
    if (!t_key) return {};

    const char *t_p = t_key + 6; // past "text"
    while (*t_p == ' ' || *t_p == ':' || *t_p == '\t') t_p++;
    if (*t_p != '"') return {};
    t_p++; // skip opening quote

    std::string t_text;
    while (*t_p && *t_p != '"')
    {
        if (*t_p == '\\' && *(t_p + 1)) { t_p++; t_text += *t_p; }
        else                             { t_text += *t_p; }
        t_p++;
    }
    return t_text;
}

////////////////////////////////////////////////////////////////////////////////
// Case-insensitive substring match

static bool _text_contains(const std::string& p_text, const std::string& p_phrase)
{
    if (p_phrase.empty()) return false;

    std::string t_text_lc   = p_text;
    std::string t_phrase_lc = p_phrase;
    for (char& c : t_text_lc)   c = (char)tolower((unsigned char)c);
    for (char& c : t_phrase_lc) c = (char)tolower((unsigned char)c);

    return t_text_lc.find(t_phrase_lc) != std::string::npos;
}

////////////////////////////////////////////////////////////////////////////////
// Main-thread callback delivery via g_idle_add

struct SpeechCallbackCtx
{
    enum Type { VOICE_COMMAND, WAKE_WORD, TIMEOUT, UNRECOGNIZED, STARTED, FAILED } type;
    std::string text;
};

static gboolean _speech_callback_idle(gpointer p_data)
{
    SpeechCallbackCtx *t_ctx = static_cast<SpeechCallbackCtx*>(p_data);

    MCStringRef t_mcstr = nil;
    if (!t_ctx->text.empty())
        /* UNCHECKED */ MCStringCreateWithCString(t_ctx->text.c_str(), t_mcstr);

    switch (t_ctx->type)
    {
        case SpeechCallbackCtx::VOICE_COMMAND:
            if (t_mcstr) MCSpeechDispatchVoiceCommand(t_mcstr);
            break;
        case SpeechCallbackCtx::WAKE_WORD:
            MCSpeechDispatchWakeWordDetected();
            break;
        case SpeechCallbackCtx::TIMEOUT:
            MCSpeechDispatchListenTimeoutExpired();
            break;
        case SpeechCallbackCtx::UNRECOGNIZED:
            if (t_mcstr) MCSpeechDispatchUnrecognizedInput(t_mcstr);
            break;
        case SpeechCallbackCtx::STARTED:
            MCSpeechDispatchListeningStarted();
            break;
        case SpeechCallbackCtx::FAILED:
            if (t_mcstr) MCSpeechDispatchListeningFailed(t_mcstr);
            break;
    }

    if (t_mcstr) MCValueRelease(t_mcstr);
    delete t_ctx;
    return G_SOURCE_REMOVE;
}

static void _dispatch(SpeechCallbackCtx::Type p_type, const std::string& p_text = {})
{
    SpeechCallbackCtx *t_ctx = new (std::nothrow) SpeechCallbackCtx{ p_type, p_text };
    if (t_ctx)
        g_idle_add(_speech_callback_idle, t_ctx);
}

////////////////////////////////////////////////////////////////////////////////
// Command-window timeout (fires on the GLib main loop)

static gboolean _on_window_timeout(gpointer /*unused*/)
{
    pthread_mutex_lock(&s_mutex);
    s_window_open    = false;
    s_timeout_src_id = 0;
    pthread_mutex_unlock(&s_mutex);

    _dispatch(SpeechCallbackCtx::TIMEOUT);
    return G_SOURCE_REMOVE;
}

// Must be called with s_mutex held.
static void _cancel_window_timer()
{
    if (s_timeout_src_id != 0)
    {
        g_source_remove(s_timeout_src_id);
        s_timeout_src_id = 0;
    }
    s_window_open = false;
}

////////////////////////////////////////////////////////////////////////////////
// Transcript handler — called from the background thread with a local copy of
// shared state (to minimise lock hold time).

static void _handle_transcript(const std::string& p_text,
                                const std::string& p_wake_word,
                                bool               p_window_open,
                                const std::vector<std::string>& p_phrases,
                                uint32_t           p_timeout_ms)
{
    // "[unk]" means speech was heard but not in the grammar.
    bool t_unrecognized = (p_text == "[unk]");

    if (t_unrecognized || p_text.empty())
    {
        // Dispatch unrecognizedVoiceCommand only in command mode, not while
        // waiting for the wake word.
        if (!t_unrecognized) return;
        if (p_wake_word.empty() || p_window_open)
            _dispatch(SpeechCallbackCtx::UNRECOGNIZED, p_text);
        return;
    }

    // ── Wake word mode: waiting for the wake word ─────────────────────────────
    if (!p_wake_word.empty() && !p_window_open)
    {
        if (_text_contains(p_text, p_wake_word))
        {
            pthread_mutex_lock(&s_mutex);
            s_window_open    = true;
            s_timeout_src_id = g_timeout_add(p_timeout_ms, _on_window_timeout, nullptr);
            pthread_mutex_unlock(&s_mutex);

            _dispatch(SpeechCallbackCtx::WAKE_WORD);
        }
        // Not the wake word — silently ignore.
        return;
    }

    // ── Command mode ──────────────────────────────────────────────────────────
    for (const auto& t_phrase : p_phrases)
    {
        if (_text_contains(p_text, t_phrase))
        {
            pthread_mutex_lock(&s_mutex);
            _cancel_window_timer();
            pthread_mutex_unlock(&s_mutex);

            _dispatch(SpeechCallbackCtx::VOICE_COMMAND, t_phrase);
            return; // first match wins
        }
    }

    // Nothing matched.
    pthread_mutex_lock(&s_mutex);
    _cancel_window_timer();
    pthread_mutex_unlock(&s_mutex);

    _dispatch(SpeechCallbackCtx::UNRECOGNIZED, p_text);
}

////////////////////////////////////////////////////////////////////////////////
// Background recognition thread

struct ThreadArgs
{
    std::string locale_id;
    bool        async_start; // true → fire listeningStarted / listeningFailed
};

static void* _recognition_thread(void *p_arg)
{
    ThreadArgs *t_args    = static_cast<ThreadArgs*>(p_arg);
    bool        t_async   = t_args->async_start;
    delete t_args;

    // ── Open the microphone ───────────────────────────────────────────────────
    pa_sample_spec t_spec;
    t_spec.format   = PA_SAMPLE_S16LE;
    t_spec.rate     = 16000;
    t_spec.channels = 1;

    int t_pa_err = 0;
    pa_simple *t_pa = s_pa_new(nullptr, "HyperXTalk", PA_STREAM_RECORD,
                                nullptr, "Speech Recognition",
                                &t_spec, nullptr, nullptr, &t_pa_err);
    if (!t_pa)
    {
        if (t_async)
            _dispatch(SpeechCallbackCtx::FAILED,
                      "startListening: could not open microphone (PulseAudio error)");
        pthread_mutex_lock(&s_mutex);
        s_is_listening   = false;
        s_thread_running = false;
        pthread_mutex_unlock(&s_mutex);
        return nullptr;
    }

    // ── Create the Vosk recognizer with the initial grammar ───────────────────
    pthread_mutex_lock(&s_mutex);
    std::string              t_wake_word  = s_wake_word;
    std::vector<std::string> t_phrases    = s_phrases;
    uint32_t                 t_timeout_ms = s_wake_timeout_ms;
    bool                     t_window     = s_window_open;
    pthread_mutex_unlock(&s_mutex);

    std::string t_grammar = (!t_wake_word.empty() && !t_window)
        ? _build_grammar({}, t_wake_word)   // wake-word-only
        : _build_grammar(t_phrases);        // full command set

    VoskRecognizer *t_rec = s_vosk_rec_new(s_model, 16000.0f, t_grammar.c_str());
    if (!t_rec)
    {
        s_pa_free(t_pa);
        if (t_async)
            _dispatch(SpeechCallbackCtx::FAILED,
                      "startListening: could not create speech recognizer");
        pthread_mutex_lock(&s_mutex);
        s_is_listening   = false;
        s_thread_running = false;
        pthread_mutex_unlock(&s_mutex);
        return nullptr;
    }

    if (t_async)
        _dispatch(SpeechCallbackCtx::STARTED);

    // ── Main capture loop ─────────────────────────────────────────────────────
    //
    // 4096 samples × 2 bytes/sample = 8192 bytes = 256 ms at 16 kHz.
    static const int k_buf_bytes = 4096 * 2;
    int16_t t_buf[4096];

    while (!s_stop_requested)
    {
        int t_err = 0;
        if (s_pa_read(t_pa, t_buf, k_buf_bytes, &t_err) < 0)
            break;

        // Check whether the phrase list, wake word, or window state has changed.
        // If so, rebuild the recognizer with the new grammar so future utterances
        // match the updated set.
        pthread_mutex_lock(&s_mutex);
        bool t_new_window = s_window_open;
        bool t_changed    = (s_phrases != t_phrases || s_wake_word != t_wake_word
                             || t_new_window != t_window);
        if (t_changed)
        {
            t_phrases    = s_phrases;
            t_wake_word  = s_wake_word;
            t_timeout_ms = s_wake_timeout_ms;
            t_window     = t_new_window;
        }
        pthread_mutex_unlock(&s_mutex);

        if (t_changed)
        {
            s_vosk_rec_free(t_rec);
            t_grammar = (!t_wake_word.empty() && !t_window)
                ? _build_grammar({}, t_wake_word)
                : _build_grammar(t_phrases);
            t_rec = s_vosk_rec_new(s_model, 16000.0f, t_grammar.c_str());
            if (!t_rec) break;
        }

        // Feed the audio chunk to Vosk.
        int t_final = s_vosk_accept(t_rec, (const char*)t_buf, k_buf_bytes);

        if (t_final == 1)
        {
            // Final result available.
            const char *t_json = s_vosk_result(t_rec);
            std::string t_text = _extract_text(t_json);

            // Snapshot shared state for the handler (avoids holding the lock
            // while dispatching callbacks).
            pthread_mutex_lock(&s_mutex);
            bool t_win  = s_window_open;
            std::vector<std::string> t_ph = s_phrases;
            std::string t_ww = s_wake_word;
            uint32_t t_tms   = s_wake_timeout_ms;
            pthread_mutex_unlock(&s_mutex);

            if (!t_text.empty())
                _handle_transcript(t_text, t_ww, t_win, t_ph, t_tms);

            // After handling, rebuild grammar if window state changed
            // (e.g. wake word was just detected and window is now open).
            pthread_mutex_lock(&s_mutex);
            bool t_post_window = s_window_open;
            pthread_mutex_unlock(&s_mutex);

            if (t_post_window != t_window)
            {
                t_window = t_post_window;
                s_vosk_rec_free(t_rec);
                t_grammar = (!t_wake_word.empty() && !t_window)
                    ? _build_grammar({}, t_wake_word)
                    : _build_grammar(t_phrases);
                t_rec = s_vosk_rec_new(s_model, 16000.0f, t_grammar.c_str());
                if (!t_rec) break;
            }
        }
        // t_final == 0  → partial result, keep accumulating
        // t_final == -1 → silence/speech boundary, Vosk handles internally
    }

    // Flush remaining audio on stop.
    if (t_rec)
    {
        s_vosk_final(t_rec);
        s_vosk_rec_free(t_rec);
    }
    s_pa_free(t_pa);

    pthread_mutex_lock(&s_mutex);
    s_is_listening   = false;
    s_thread_running = false;
    pthread_mutex_unlock(&s_mutex);

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Platform entry points

bool MCPlatformStartListening(MCStringRef p_language)
{
    if (!_load_pulse())
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: PulseAudio (libpulse-simple) is not available", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    if (!_load_vosk())
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: Vosk speech recognition library (libvosk) is not available. "
            "Install it from https://alphacephei.com/vosk/ or via 'pip install vosk'", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    pthread_mutex_lock(&s_mutex);
    if (s_is_listening)
    {
        pthread_mutex_unlock(&s_mutex);
        return true;
    }
    pthread_mutex_unlock(&s_mutex);

    s_vosk_log(-1); // suppress Vosk console output

    if (!s_model)
    {
        std::string t_model_path = _find_model_path();
        if (t_model_path.empty())
        {
            MCStringRef t_err;
            /* UNCHECKED */ MCStringCreateWithCString(
                "startListening: no Vosk model found. "
                "Download a model from https://alphacephei.com/vosk/models "
                "and place it at ~/.local/share/vosk/model, "
                "or set $VOSK_MODEL_PATH", t_err);
            MCresult->setvalueref(t_err);
            MCValueRelease(t_err);
            return false;
        }
        s_model = s_vosk_model_new(t_model_path.c_str());
        if (!s_model)
        {
            MCStringRef t_err;
            /* UNCHECKED */ MCStringCreateWithCString(
                "startListening: failed to load Vosk model — "
                "check that the model directory is valid", t_err);
            MCresult->setvalueref(t_err);
            MCValueRelease(t_err);
            return false;
        }
    }

    // Extract locale string (informational — Vosk uses the model's locale).
    std::string t_locale_id;
    if (p_language != nil && !MCStringIsEmpty(p_language))
    {
        char *t_cstr = nil;
        if (MCStringConvertToCString(p_language, t_cstr))
        {
            t_locale_id = t_cstr;
            MCMemoryDeallocate(t_cstr);
        }
    }

    pthread_mutex_lock(&s_mutex);
    s_is_listening   = true;
    s_stop_requested = false;
    s_window_open    = false;
    pthread_mutex_unlock(&s_mutex);

    ThreadArgs *t_args = new (std::nothrow) ThreadArgs{ t_locale_id, false };
    if (!t_args)
    {
        pthread_mutex_lock(&s_mutex);
        s_is_listening = false;
        pthread_mutex_unlock(&s_mutex);
        return false;
    }

    if (pthread_create(&s_thread, nullptr, _recognition_thread, t_args) != 0)
    {
        delete t_args;
        pthread_mutex_lock(&s_mutex);
        s_is_listening   = false;
        s_thread_running = false;
        pthread_mutex_unlock(&s_mutex);

        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: failed to start recognition thread", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    pthread_mutex_lock(&s_mutex);
    s_thread_running = true;
    pthread_mutex_unlock(&s_mutex);

    pthread_detach(s_thread);
    return true;
}

void MCPlatformStopListening()
{
    pthread_mutex_lock(&s_mutex);
    if (!s_is_listening)
    {
        pthread_mutex_unlock(&s_mutex);
        return;
    }
    s_stop_requested = true;
    _cancel_window_timer();
    s_wake_word.clear();
    pthread_mutex_unlock(&s_mutex);
    // The background thread will notice s_stop_requested and exit naturally on
    // the next pa_simple_read call, then clean up its own resources.
}

bool MCPlatformIsListening()
{
    pthread_mutex_lock(&s_mutex);
    bool t_result = s_is_listening;
    pthread_mutex_unlock(&s_mutex);
    return t_result;
}

void MCPlatformSetVoiceCommands(MCStringRef p_phrases)
{
    std::vector<std::string> t_new_phrases;

    if (p_phrases != nil && !MCStringIsEmpty(p_phrases))
    {
        char *t_cstr = nil;
        if (MCStringConvertToCString(p_phrases, t_cstr))
        {
            const char *t_p     = t_cstr;
            const char *t_start = t_cstr;
            while (true)
            {
                bool t_end = (*t_p == '\0');
                if (*t_p == '\n' || t_end)
                {
                    if (t_p > t_start)
                        t_new_phrases.emplace_back(t_start, (size_t)(t_p - t_start));
                    if (t_end) break;
                    t_start = t_p + 1;
                }
                t_p++;
            }
            MCMemoryDeallocate(t_cstr);
        }
    }

    pthread_mutex_lock(&s_mutex);
    s_phrases = t_new_phrases;
    // The background thread detects the change on the next loop iteration
    // and rebuilds the Vosk recognizer with the updated grammar.
    pthread_mutex_unlock(&s_mutex);
}

void MCPlatformSetWakeWord(MCStringRef p_word, uint32_t p_timeout_ms)
{
    std::string t_new_word;
    if (p_word != nil && !MCStringIsEmpty(p_word))
    {
        char *t_cstr = nil;
        if (MCStringConvertToCString(p_word, t_cstr))
        {
            t_new_word = t_cstr;
            MCMemoryDeallocate(t_cstr);
        }
    }

    pthread_mutex_lock(&s_mutex);
    s_wake_word       = t_new_word;
    s_wake_timeout_ms = (p_timeout_ms > 0) ? p_timeout_ms : 5000;
    _cancel_window_timer(); // reset any active command window immediately
    pthread_mutex_unlock(&s_mutex);
    // The background thread will pick up the new wake word on the next chunk.
}
