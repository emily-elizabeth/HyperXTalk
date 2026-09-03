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
// macOS speech recognition backend using SFSpeechRecognizer + AVAudioEngine.
//
// Architecture:
//   All SFSpeechRecognizer and AVAudioEngine state lives on a single serial
//   dispatch queue (s_speech_queue).  Platform entry points called by the
//   engine are dispatched synchronously onto that queue so the engine thread
//   never touches recognition objects directly.
//
//   Recognition results arrive asynchronously on s_speech_queue via the
//   SFSpeechRecognitionTask result handler.  When a phrase matches, we use
//   MCMacPlatformScheduleCallback to deliver the engine callback on the main
//   thread (same pattern as mac-hotkey.mm).
//
//   The recognition task auto-restarts after each final result so the engine
//   always stays in a listening state once startListening is called.
//
// Recognised phrase matching:
//   - contextualStrings biases SFSpeechRecognizer toward registered phrases.
//   - After transcription we compare the recognised text case-insensitively
//     against each registered phrase (substring match so minor filler words
//     like "uh" at the start don't break things).
//
// Wake word:
//   When a wake word is set the engine starts in "wake word mode":
//     • Only the wake word phrase is matched.
//     • On match → wakeWordDetected dispatched + command window timer started.
//     • During the window registered commands are matched normally.
//     • If the window expires → listenTimeoutExpired dispatched + back to
//       wake-word-only mode.
//   When no wake word is set every recognised phrase fires voiceCommand directly.
//
// Requires:
//   -framework Speech  (SFSpeechRecognizer, macOS 10.15+)
//   -framework AVFoundation  (AVAudioEngine)
//   -framework Foundation
//

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <Speech/Speech.h>

#include <string>
#include <vector>

#include "prefix.h"
#include "mcstring.h"
#include "param.h"
#include "globals.h"
#include "speech.h"

// Forward-declare; see mac-hotkey.mm for the full implementation.
void MCMacPlatformScheduleCallback(void (*)(void *), void *);

////////////////////////////////////////////////////////////////////////////////
// State (all access must be from s_speech_queue)

static dispatch_queue_t                        s_speech_queue     = nil;
static SFSpeechRecognizer                     *s_recognizer       = nil;
static AVAudioEngine                          *s_engine           = nil;
static SFSpeechAudioBufferRecognitionRequest  *s_request          = nil;
static SFSpeechRecognitionTask                *s_task             = nil;
static bool                                    s_is_listening     = false;

// Phrase list — updated by MCPlatformSetVoiceCommands.
// Stored as C++ strings to avoid ObjC retain/release issues in non-ARC builds.
static std::vector<std::string>                s_phrases;

// Wake word state.
// Also stored as a C++ string for the same reason.
static std::string                             s_wake_word;
static uint32_t                                s_wake_timeout_ms  = 5000;
static bool                                    s_window_open      = false; // command window active
static dispatch_source_t                       s_timeout_timer    = nil;

// Silence-detection fallback.
// On macOS 14+ / Tahoe, isFinal=YES from SFSpeechRecognizer may not arrive
// until the user stops speaking and the OS detects a long silence.  For voice
// command recognition we want faster response.  We maintain the last partial
// transcript and a 1.5 s timer; when the timer fires (= no new partials for
// 1.5 s) we treat the pending transcript as the final result.
// Both variables are guarded by s_speech_queue.
static std::string                             s_pending_transcript;
static dispatch_source_t                       s_silence_timer    = nil;
static uint32_t                                s_task_generation  = 0;

////////////////////////////////////////////////////////////////////////////////
// Helpers

// Ensure the serial queue exists.
static void _ensure_queue()
{
    if (s_speech_queue == nil)
        s_speech_queue = dispatch_queue_create("com.hyperxtalk.speech", DISPATCH_QUEUE_SERIAL);
}

// Cancel the command-window timer if running.
// Must be called from s_speech_queue.
static void _cancel_timer()
{
    if (s_timeout_timer != nil)
    {
        dispatch_source_cancel(s_timeout_timer);
        s_timeout_timer = nil;
    }
    s_window_open = false;
}

// Cancel the silence-detection timer.  Must be called from s_speech_queue.
static void _cancel_silence_timer()
{
    if (s_silence_timer != nil)
    {
        dispatch_source_cancel(s_silence_timer);
        s_silence_timer = nil;
    }
}

// Stop the AVAudioEngine and cancel the recognition task.
// Must be called from s_speech_queue.
static void _stop_engine()
{
    _cancel_timer();
    _cancel_silence_timer();
    s_pending_transcript.clear();

    if (s_task != nil)
    {
        [s_task cancel];
        [s_task release];
        s_task = nil;
    }
    if (s_request != nil)
    {
        [s_request endAudio];
        [s_request release];
        s_request = nil;
    }
    if (s_engine != nil && s_engine.running)
    {
        [s_engine.inputNode removeTapOnBus:0];
        [s_engine stop];
    }
}

// Build an NSArray of phrases to pass as contextualStrings.
// Includes the wake word (if set) so it is biased too.
// Converts from C++ strings on the fly — no static ObjC objects involved.
static NSArray<NSString *> *_build_context_strings()
{
    NSMutableArray *t_all = [NSMutableArray array];
    for (const auto& t_p : s_phrases)
    {
        NSString *t_ns = [NSString stringWithUTF8String:t_p.c_str()];
        if (t_ns) [t_all addObject:t_ns];
    }
    if (!s_wake_word.empty())
    {
        NSString *t_ww = [NSString stringWithUTF8String:s_wake_word.c_str()];
        if (t_ww && ![t_all containsObject:t_ww])
            [t_all addObject:t_ww];
    }
    return t_all;
}

// ── Matching helpers ──────────────────────────────────────────────────────────

// Returns YES if the recognised transcript contains p_phrase as a
// case-insensitive whole-word sequence.
//
// Word-boundary matching prevents short phrases from false-matching mid-word:
// "no" must not fire on "I don't know", "yes" must not fire on "yesterday".
// NSRegularExpression \b honours Unicode word boundaries.  If regex creation
// fails (empty phrase, special chars only) we fall back to a plain substring
// search so the function never silently drops a phrase.
static BOOL _transcript_contains(NSString *p_transcript, NSString *p_phrase)
{
    NSString *t_escaped = [NSRegularExpression escapedPatternForString:p_phrase];
    NSString *t_pattern = [NSString stringWithFormat:@"(?i)\\b%@\\b", t_escaped];
    NSRegularExpression *t_re =
        [NSRegularExpression regularExpressionWithPattern:t_pattern options:0 error:nil];
    if (t_re != nil)
    {
        return [t_re numberOfMatchesInString:p_transcript
                                     options:0
                                       range:NSMakeRange(0, p_transcript.length)] > 0;
    }
    // Fallback: plain case-insensitive substring (original behaviour).
    return [p_transcript rangeOfString:p_phrase
                               options:NSCaseInsensitiveSearch].location != NSNotFound;
}

// ── Dispatch helpers (called from s_speech_queue, cross to main thread) ──────

struct MCDispatchVCCtx
{
    MCStringRef phrase;
};

static void _dispatch_voice_command(void *p_ctx)
{
    MCDispatchVCCtx *t_ctx = (MCDispatchVCCtx *)p_ctx;
    MCSpeechDispatchVoiceCommand(t_ctx->phrase);
    MCValueRelease(t_ctx->phrase);
    delete t_ctx;
}

static void _dispatch_voice_command_on_main(NSString *p_phrase)
{
    const char *t_utf8 = [p_phrase UTF8String];
    if (t_utf8 == nil)
        return;

    MCStringRef t_mc_phrase = nil;
    if (!MCStringCreateWithCString(t_utf8, t_mc_phrase))
        return;

    MCDispatchVCCtx *t_ctx = new (std::nothrow) MCDispatchVCCtx{ t_mc_phrase };
    if (t_ctx == nil)
    {
        MCValueRelease(t_mc_phrase);
        return;
    }
    MCMacPlatformScheduleCallback(_dispatch_voice_command, t_ctx);
}

static void _dispatch_wake_word_on_main(void * /*unused*/)
{
    MCSpeechDispatchWakeWordDetected();
}

static void _dispatch_timeout_on_main(void * /*unused*/)
{
    MCSpeechDispatchListenTimeoutExpired();
}

// ── Recognition task restart ─────────────────────────────────────────────────

// Forward declarations.
static void _start_recognition_task();
static void _handle_transcript(NSString *p_transcript);

// Dispatch the pending transcript (if any) to the matching logic, then restart
// the recognition task.  Must be called from s_speech_queue.
static void _process_pending_and_restart()
{
    _cancel_silence_timer();
    if (s_is_listening && !s_pending_transcript.empty())
    {
        NSString *t_ns = [NSString stringWithUTF8String:s_pending_transcript.c_str()];
        s_pending_transcript.clear();
        if (t_ns) _handle_transcript(t_ns);
    }
    else
    {
        s_pending_transcript.clear();
    }
    if (s_is_listening)
        _start_recognition_task();
}

// Schedule (or reset) the silence-detection timer.  Must be called from s_speech_queue.
// When it fires, the pending transcript is processed as if isFinal=YES had arrived.
static void _reset_silence_timer()
{
    _cancel_silence_timer();
    if (!s_is_listening || s_pending_transcript.empty())
        return;

    s_silence_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, s_speech_queue);
    dispatch_source_set_timer(s_silence_timer,
        dispatch_time(DISPATCH_TIME_NOW, 1500 * NSEC_PER_MSEC), // 1.5 s of silence
        DISPATCH_TIME_FOREVER, 100 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(s_silence_timer, ^{
        _process_pending_and_restart();
    });
    dispatch_resume(s_silence_timer);
}

// Process one final recognised transcript on s_speech_queue.
static void _handle_transcript(NSString *p_transcript)
{
    if (!s_is_listening)
        return;

    // ── Wake word mode: waiting for the wake word ─────────────────────────────
    if (!s_wake_word.empty() && !s_window_open)
    {
        NSString *t_ww_ns = [NSString stringWithUTF8String:s_wake_word.c_str()];
        BOOL t_ww_matched = t_ww_ns && _transcript_contains(p_transcript, t_ww_ns);
        if (t_ww_matched)
        {
            s_window_open = true;
            MCMacPlatformScheduleCallback(_dispatch_wake_word_on_main, nil);

            // Start command-window timer.
            s_timeout_timer = dispatch_source_create(
                DISPATCH_SOURCE_TYPE_TIMER, 0, 0, s_speech_queue);
            dispatch_source_set_timer(
                s_timeout_timer,
                dispatch_time(DISPATCH_TIME_NOW, (int64_t)s_wake_timeout_ms * NSEC_PER_MSEC),
                DISPATCH_TIME_FOREVER,
                1 * NSEC_PER_MSEC);
            dispatch_source_set_event_handler(s_timeout_timer, ^{
                _cancel_timer();
                MCMacPlatformScheduleCallback(_dispatch_timeout_on_main, nil);
            });
            dispatch_resume(s_timeout_timer);
        }
        return; // not a wake word match — ignore
    }

    // ── Command mode: no wake word, or window is open ─────────────────────────
    for (const auto& t_phrase_str : s_phrases)
    {
        NSString *t_phrase_ns = [NSString stringWithUTF8String:t_phrase_str.c_str()];
        if (t_phrase_ns && _transcript_contains(p_transcript, t_phrase_ns))
        {
            // If we were in a wake-word command window, close it.
            _cancel_timer();
            _dispatch_voice_command_on_main(t_phrase_ns);
            return; // first match wins
        }
    }

    // Nothing matched — dispatch unrecognizedVoiceCommand with the raw transcript
    // so the caller can forward it to an LLM or handle it otherwise.
    // If we were in a wake-word command window, close it first.
    if (s_window_open)
        _cancel_timer();

    const char *t_utf8 = [p_transcript UTF8String];
    if (t_utf8 != nil)
    {
        MCStringRef t_mc_text = nil;
        if (MCStringCreateWithCString(t_utf8, t_mc_text))
        {
            struct MCDispatchURICtx { MCStringRef text; };
            MCDispatchURICtx *t_ctx = new (std::nothrow) MCDispatchURICtx{ t_mc_text };
            if (t_ctx != nil)
            {
                MCMacPlatformScheduleCallback([](void *p_ctx) {
                    MCDispatchURICtx *t_c = (MCDispatchURICtx *)p_ctx;
                    MCSpeechDispatchUnrecognizedInput(t_c->text);
                    MCValueRelease(t_c->text);
                    delete t_c;
                }, t_ctx);
            }
            else
            {
                MCValueRelease(t_mc_text);
            }
        }
    }
}


// (Re)start a recognition task.  Must be called from s_speech_queue.
//
// The audio tap is installed ONCE at engine-startup time and reads s_request
// via the global at call time. This function only swaps out the request + task
// objects; the engine and its tap keep running without interruption.
static void _start_recognition_task()
{
    // Cancel any in-flight task/request.
    // s_task is retained on assignment (recognitionTaskWithRequest: returns autoreleased);
    // release it here before clearing so it isn't a dangling pointer.
    if (s_task != nil)
    {
        [s_task cancel];
        [s_task release];
        s_task = nil;
    }
    if (s_request != nil)
    {
        [s_request endAudio];
        [s_request release];
        s_request = nil;
    }

    s_request = [[SFSpeechAudioBufferRecognitionRequest alloc] init];
    if (s_request == nil)
        return;

    // Partial results required on macOS Tahoe: isFinal=YES arrives with an empty
    // bestTranscription.formattedString.  The actual text is in preceding partials.
    // We accumulate partials in s_pending_transcript (a global on s_speech_queue)
    // and run a 1.5-second silence timer so we don't wait for a natural isFinal.
    s_request.shouldReportPartialResults = YES;
    s_request.taskHint = SFSpeechRecognitionTaskHintSearch; // short command phrases
    s_request.contextualStrings = _build_context_strings();

    // Increment the generation counter so that any result-handler callback
    // belonging to a previous (cancelled) task can detect it has been superseded
    // and bail out, preventing the cancellation-error cascade that kills new tasks.
    uint32_t t_gen = ++s_task_generation;

    // recognitionTaskWithRequest: returns an autoreleased object (NARC).
    // Retain explicitly; released in _stop_engine and at the top of this function.
    s_task = [[s_recognizer recognitionTaskWithRequest:s_request
                                        resultHandler:^(SFSpeechRecognitionResult *r, NSError *e) {

        if (r != nil)
        {
            NSString *t_str = r.bestTranscription.formattedString;

            if (!r.isFinal)
            {
                // Partial result: update s_pending_transcript and reset silence timer.
                if (t_str.length > 0)
                {
                    std::string t_partial(t_str.UTF8String);
                    dispatch_async(s_speech_queue, ^{
                        if (!s_is_listening || s_task_generation != t_gen) return;
                        s_pending_transcript = t_partial;
                        _reset_silence_timer();
                    });
                }
                return;
            }

            // isFinal=YES: cancel silence timer and use the final string (or pending
            // fallback if the final is empty, as happens on macOS Tahoe).
            std::string t_final = (t_str.length > 0)
                ? std::string(t_str.UTF8String)
                : std::string(); // let _process_pending_and_restart use s_pending_transcript

            dispatch_async(s_speech_queue, ^{
                if (!s_is_listening || s_task_generation != t_gen) return;
                if (!t_final.empty())
                    s_pending_transcript = t_final;
                _process_pending_and_restart();
            });
            return;
        }

        if (e != nil)
        {
            // Task ended with error (silence timeout, cancellation, etc.).
            // Only restart if this is still the current task — a cancellation error
            // from a superseded task must not kill the task that replaced it.
            dispatch_async(s_speech_queue, ^{
                if (!s_is_listening || s_task_generation != t_gen) return;
                _process_pending_and_restart();
            });
        }
    }] retain];
}

////////////////////////////////////////////////////////////////////////////////
// Platform entry points

// ── Engine startup helper ─────────────────────────────────────────────────────
// MUST be called from s_speech_queue. Both permissions must be Authorized.
// p_locale_id is a BCP-47 locale identifier string; empty string → system default.
// Using a C++ string avoids any ObjC retain/autorelease concern when called from
// an async completion handler (non-ARC blocks don't retain captured ObjC objects).
// Returns true on success; writes a literal C string to *r_reason on failure.
static bool _start_engine_locked(const std::string& p_locale_id, const char **r_reason)
{
    if (s_is_listening)
        return true; // harmless race

    // Build the NSLocale right here, where it's used — short-lived, no cross-
    // boundary lifetime concern.
    NSLocale *t_locale = nil;
    if (!p_locale_id.empty())
    {
        NSString *t_id = [NSString stringWithUTF8String:p_locale_id.c_str()];
        if (t_id.length > 0)
            t_locale = [NSLocale localeWithLocaleIdentifier:t_id];
    }

    s_recognizer = t_locale
        ? [[SFSpeechRecognizer alloc] initWithLocale:t_locale]
        : [[SFSpeechRecognizer alloc] init];

    if (s_recognizer == nil || !s_recognizer.available)
    {
        if (r_reason) *r_reason = "startListening: speech recogniser unavailable for this locale";
        s_recognizer = nil;
        return false;
    }

    // Set up the audio engine.
    // Order matters on macOS:
    //   1. prepare — allocates hardware resources and makes the input node's
    //      format valid (outputFormatForBus:0 returns 0 sample-rate before this).
    //   2. installTapOnBus — must be done before start, and after prepare so
    //      the format we pass is the real hardware PCM format.
    //   3. startAndReturnError — begins audio flow.
    // The tap reads s_request via the global at call time, so swapping s_request
    // in _start_recognition_task() needs no tap reinstall.
    s_engine = [[AVAudioEngine alloc] init];

    // Do NOT call [s_engine prepare] before installing the tap.  On macOS 14+
    // (and Tahoe) prepare can throw NSException before the tap is in place.
    // startAndReturnError: auto-prepares internally, so explicit prepare is
    // unnecessary.
    //
    // Use inputFormatForBus:0 (the hardware's native PCM capture format) instead
    // of outputFormatForBus:0.  The output-bus format may report a zero sample
    // rate before the engine is started; the input-bus format is always valid
    // because it reflects the physical audio device.
    //
    // Both prepare and startAndReturnError: can throw NSException on macOS
    // rather than returning NO; wrap the entire setup in @try/@catch.
    BOOL              t_started = NO;
    AVAudioInputNode *t_input   = nil;
    @try
    {
        t_input = s_engine.inputNode;
        AVAudioFormat *t_fmt = [t_input inputFormatForBus:0];
        [t_input installTapOnBus:0 bufferSize:4096 format:t_fmt
                           block:^(AVAudioPCMBuffer *p_buf, AVAudioTime *) {
            if (s_request != nil)
                [s_request appendAudioPCMBuffer:p_buf];
        }];

        NSError *t_err = nil;
        t_started = [s_engine startAndReturnError:&t_err];
    }
    @catch (NSException *) { t_started = NO; }

    if (!t_started)
    {
        if (t_input != nil) [t_input removeTapOnBus:0];
        if (r_reason) *r_reason = "startListening: audio engine failed to start";
        s_engine     = nil;
        s_recognizer = nil;
        return false;
    }

    s_is_listening = true;
    _start_recognition_task();
    return true;
}

// ── Async permission + start ──────────────────────────────────────────────────
// Pure completion-handler chain — no semaphores, no threads blocked.
// requestAuthorization's callback is on the main thread; requestAccessForMediaType's
// is on an arbitrary queue. Neither requires the calling thread to wait.
// Kicks off the chain and returns immediately; fires listeningStarted or
// listeningFailed pReason on the card when done.
//
// p_locale_id is passed by value — the C++ string is owned by the lambda/block
// capture and lives for the full duration of the async chain without any ObjC
// retain/autorelease involvement (non-ARC blocks do NOT retain ObjC objects).
static void _request_permissions_and_start(std::string p_locale_id)
{
    // Capture p_locale_id by moving into a heap-allocated copy so nested ObjC
    // blocks can safely reference it.  __block gives the block mutable access
    // and keeps the value alive until the last block referencing it is released.
    __block std::string t_locale_id = std::move(p_locale_id);

    [SFSpeechRecognizer requestAuthorization:^(SFSpeechRecognizerAuthorizationStatus t_sr) {

        if (t_sr != SFSpeechRecognizerAuthorizationStatusAuthorized)
        {
            MCMacPlatformScheduleCallback([](void *) {
                MCStringRef t_s = nil;
                if (MCStringCreateWithCString(
                        "startListening: speech recognition permission denied", t_s))
                {
                    MCSpeechDispatchListeningFailed(t_s);
                    MCValueRelease(t_s);
                }
            }, nil);
            return;
        }

        // Speech recognition granted — now request microphone.
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                                completionHandler:^(BOOL t_mic_granted) {

            if (!t_mic_granted)
            {
                MCMacPlatformScheduleCallback([](void *) {
                    MCStringRef t_s = nil;
                    if (MCStringCreateWithCString(
                            "startListening: microphone permission denied", t_s))
                    {
                        MCSpeechDispatchListeningFailed(t_s);
                        MCValueRelease(t_s);
                    }
                }, nil);
                return;
            }

            // Both granted — start engine on s_speech_queue.
            // t_locale_id is a C++ std::string captured via __block — no ObjC
            // retain/autorelease involved, so it is safe across the async boundary.
            __block bool        t_ok     = false;
            __block const char *t_reason = nil;
            dispatch_sync(s_speech_queue, ^{
                t_ok = _start_engine_locked(t_locale_id, &t_reason);
            });

            if (t_ok)
            {
                MCMacPlatformScheduleCallback([](void *) {
                    MCSpeechDispatchListeningStarted();
                }, nil);
            }
            else
            {
                struct FailCtx { MCStringRef s; };
                const char *t_msg = t_reason ? t_reason
                                             : "startListening: audio engine failed to start";
                MCStringRef t_s   = nil;
                FailCtx    *t_ctx = nil;
                if (MCStringCreateWithCString(t_msg, t_s))
                {
                    t_ctx = new (std::nothrow) FailCtx{ t_s };
                    if (!t_ctx) MCValueRelease(t_s);
                }
                MCMacPlatformScheduleCallback([](void *p) {
                    auto *c = static_cast<FailCtx *>(p);
                    if (c)
                    {
                        if (c->s) { MCSpeechDispatchListeningFailed(c->s); MCValueRelease(c->s); }
                        delete c;
                    }
                }, t_ctx);
            }
        }]; // requestAccessForMediaType
    }]; // requestAuthorization
}

bool MCPlatformStartListening(MCStringRef p_language)
{
    // SFSpeechRecognizer requires macOS 10.15+.  Weak linking lets the engine
    // load on older systems; we just return an error at runtime instead.
    if (@available(macOS 10.15, *))
    {
        // supported — fall through to the implementation below
    }
    else
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: speech recognition requires macOS 10.15 or later", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    _ensure_queue();

    // Quick check — already listening?
    __block bool t_already = false;
    dispatch_sync(s_speech_queue, ^{ t_already = s_is_listening; });
    if (t_already)
        return true;

    // Check current permission state (no blocking — just reading cached OS state).
    SFSpeechRecognizerAuthorizationStatus t_sr =
        [SFSpeechRecognizer authorizationStatus];
    AVAuthorizationStatus t_mic =
        [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];

    // Fail fast on explicit denial.
    if (t_sr == SFSpeechRecognizerAuthorizationStatusDenied ||
        t_sr == SFSpeechRecognizerAuthorizationStatusRestricted)
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: speech recognition permission denied", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }
    if (t_mic == AVAuthorizationStatusDenied || t_mic == AVAuthorizationStatusRestricted)
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(
            "startListening: microphone permission denied", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    // Extract locale identifier as a C++ string — this is safe to copy into
    // async blocks/lambdas without any ObjC retain/autorelease involvement.
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

    bool t_needs_dialog =
        (t_sr  == SFSpeechRecognizerAuthorizationStatusNotDetermined) ||
        (t_mic == AVAuthorizationStatusNotDetermined);

    if (!t_needs_dialog)
    {
        // Fast synchronous path: both permissions already granted.
        __block bool        t_ok     = false;
        __block const char *t_reason = nil;
        dispatch_sync(s_speech_queue, ^{
            t_ok = _start_engine_locked(t_locale_id, &t_reason);
        });
        if (!t_ok)
        {
            const char *t_msg = t_reason ? t_reason
                                         : "startListening: speech recognition unavailable";
            MCStringRef t_err;
            /* UNCHECKED */ MCStringCreateWithCString(t_msg, t_err);
            MCresult->setvalueref(t_err);
            MCValueRelease(t_err);
        }
        return t_ok;
    }

    // Slow async path: one or both permissions are NotDetermined — OS dialogs
    // must be shown.  _request_permissions_and_start uses a pure callback chain
    // (no semaphores, no blocked threads) so it is safe to call directly from
    // the main thread; it returns immediately and fires listeningStarted or
    // listeningFailed pReason on the card when the chain completes.
    _request_permissions_and_start(std::move(t_locale_id));
    return true; // async; listeningStarted / listeningFailed will follow
}

void MCPlatformStopListening()
{
    _ensure_queue();
    dispatch_sync(s_speech_queue, ^{
        if (!s_is_listening)
            return;
        s_is_listening = false;
        _stop_engine();      // also calls _cancel_timer → s_window_open = false
        s_wake_word.clear(); // don't carry wake-word state into the next session
        s_engine     = nil;
        s_recognizer = nil;
    });
}

bool MCPlatformIsListening()
{
    if (s_speech_queue == nil)
        return false;
    __block bool t_result = false;
    dispatch_sync(s_speech_queue, ^{ t_result = s_is_listening; });
    return t_result;
}

void MCPlatformSetVoiceCommands(MCStringRef p_phrases)
{
    // Build a C++ vector of UTF-8 strings — no ObjC memory management required.
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
                    // Trim leading/trailing whitespace from the segment.
                    const char *t_l = t_start;
                    const char *t_r = t_p - 1;
                    while (t_l <= t_r && (*t_l == ' ' || *t_l == '\t')) t_l++;
                    while (t_r >= t_l && (*t_r == ' ' || *t_r == '\t')) t_r--;
                    if (t_l <= t_r)
                        t_new_phrases.emplace_back(t_l, (size_t)(t_r - t_l + 1));
                    if (t_end) break;
                    t_start = t_p + 1;
                }
                t_p++;
            }
            MCMemoryDeallocate(t_cstr);
        }
    }

    _ensure_queue();
    dispatch_sync(s_speech_queue, ^{
        s_phrases = t_new_phrases; // C++ copy — no ObjC retain/release

        // If already listening, restart the task with the updated phrase list.
        if (s_is_listening)
            _start_recognition_task();
    });
}

void MCPlatformSetWakeWord(MCStringRef p_word, uint32_t p_timeout_ms)
{
    // Store as C++ string — no ObjC retain/release needed.
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

    _ensure_queue();
    dispatch_sync(s_speech_queue, ^{
        s_wake_word       = t_new_word; // C++ copy
        s_wake_timeout_ms = (p_timeout_ms > 0) ? p_timeout_ms : 5000;

        // Reset any active command window — setting the wake word while
        // listening takes effect immediately.
        _cancel_timer();

        if (s_is_listening)
            _start_recognition_task();
    });
}
