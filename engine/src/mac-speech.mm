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
// Each element is a single phrase string.
static NSMutableArray<NSString *>             *s_phrases          = nil;

// Wake word state.
static NSString                               *s_wake_word        = nil;
static uint32_t                                s_wake_timeout_ms  = 5000;
static bool                                    s_window_open      = false; // command window active
static dispatch_source_t                       s_timeout_timer    = nil;

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

// Stop the AVAudioEngine and cancel the recognition task.
// Must be called from s_speech_queue.
static void _stop_engine()
{
    _cancel_timer();

    if (s_task != nil)
    {
        [s_task cancel];
        s_task = nil;
    }
    if (s_request != nil)
    {
        [s_request endAudio];
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
static NSArray<NSString *> *_build_context_strings()
{
    NSMutableArray *t_all = [NSMutableArray arrayWithArray:s_phrases];
    if (s_wake_word.length > 0 && ![t_all containsObject:s_wake_word])
        [t_all addObject:s_wake_word];
    return t_all;
}

// ── Matching helpers ──────────────────────────────────────────────────────────

// Returns YES if the recognised transcript contains p_phrase as a
// case-insensitive substring.
static BOOL _transcript_contains(NSString *p_transcript, NSString *p_phrase)
{
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

// Forward declaration.
static void _start_recognition_task();

// Process one final recognised transcript on s_speech_queue.
static void _handle_transcript(NSString *p_transcript)
{
    if (!s_is_listening)
        return;

    // ── Wake word mode: waiting for the wake word ─────────────────────────────
    if (s_wake_word.length > 0 && !s_window_open)
    {
        if (_transcript_contains(p_transcript, s_wake_word))
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
    for (NSString *t_phrase in s_phrases)
    {
        if (_transcript_contains(p_transcript, t_phrase))
        {
            // If we were in a wake-word command window, close it.
            _cancel_timer();
            _dispatch_voice_command_on_main(t_phrase);
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

// Result handler called by SFSpeechRecognizer (on an internal queue).
// We bounce back to s_speech_queue for all state access.
static void _result_handler(SFSpeechRecognitionResult *p_result, NSError *p_error)
{
    // SFSpeechRecognizer may call with both a result AND an error on the final
    // call indicating the task is done.  Process any final result first.
    if (p_result != nil && p_result.isFinal)
    {
        NSString *t_transcript = p_result.bestTranscription.formattedString;
        dispatch_async(s_speech_queue, ^{
            _handle_transcript(t_transcript);
            if (s_is_listening)
                _start_recognition_task(); // restart for next utterance
        });
        return;
    }

    if (p_error != nil)
    {
        // Task ended without a final result (silence timeout, cancellation, etc.)
        // Restart silently if we're still supposed to be listening.
        dispatch_async(s_speech_queue, ^{
            if (s_is_listening)
                _start_recognition_task();
        });
    }
}

// (Re)start a recognition task.  Must be called from s_speech_queue.
static void _start_recognition_task()
{
    // Clean up any previous task/request without stopping the engine.
    if (s_task != nil)
    {
        [s_task cancel];
        s_task = nil;
    }
    if (s_request != nil)
    {
        [s_request endAudio];
        s_request = nil;
    }

    s_request = [[SFSpeechAudioBufferRecognitionRequest alloc] init];
    if (s_request == nil)
        return;

    s_request.shouldReportPartialResults = NO;  // fire only on complete utterances
    s_request.contextualStrings = _build_context_strings();

    // On-device recognition preferred (available macOS 13+, no network needed).
    if (@available(macOS 13.0, *))
        s_request.requiresOnDeviceRecognition = YES;

    // Re-install the audio tap so the new request gets audio.
    // (The engine keeps running — only the tap target changes.)
    AVAudioInputNode *t_input = s_engine.inputNode;
    [t_input removeTapOnBus:0];

    AVAudioFormat *t_format = [t_input outputFormatForBus:0];
    [t_input installTapOnBus:0 bufferSize:4096 format:t_format block:^(AVAudioPCMBuffer *p_buf, AVAudioTime *) {
        [s_request appendAudioPCMBuffer:p_buf];
    }];

    s_task = [s_recognizer recognitionTaskWithRequest:s_request
                                       resultHandler:^(SFSpeechRecognitionResult *r, NSError *e) {
        _result_handler(r, e);
    }];
}

////////////////////////////////////////////////////////////////////////////////
// Platform entry points

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

    __block bool t_success = false;
    __block const char *t_fail_reason = nil; // set inside block on failure

    dispatch_sync(s_speech_queue, ^{
        if (s_is_listening)
        {
            t_success = true; // already running
            return;
        }

        // ── 1. Build locale ───────────────────────────────────────────────────
        NSLocale *t_locale = nil;
        if (p_language != nil && !MCStringIsEmpty(p_language))
        {
            char *t_lang_cstr = nil;
            if (MCStringConvertToCString(p_language, t_lang_cstr))
            {
                NSString *t_lang_ns = [NSString stringWithUTF8String:t_lang_cstr];
                MCMemoryDeallocate(t_lang_cstr);
                if (t_lang_ns.length > 0)
                    t_locale = [NSLocale localeWithLocaleIdentifier:t_lang_ns];
            }
        }

        // ── 2. Create recogniser ──────────────────────────────────────────────
        if (t_locale != nil)
            s_recognizer = [[SFSpeechRecognizer alloc] initWithLocale:t_locale];
        else
            s_recognizer = [[SFSpeechRecognizer alloc] init];

        if (s_recognizer == nil || !s_recognizer.available)
        {
            t_fail_reason = "startListening: speech recogniser unavailable for this locale";
            s_recognizer = nil;
            return;
        }

        // ── 3. Request authorisation (may block briefly on first run) ─────────
        __block SFSpeechRecognizerAuthorizationStatus t_auth =
            SFSpeechRecognizerAuthorizationStatus::SFSpeechRecognizerAuthorizationStatusNotDetermined;

        dispatch_semaphore_t t_sem = dispatch_semaphore_create(0);
        [SFSpeechRecognizer requestAuthorization:^(SFSpeechRecognizerAuthorizationStatus status) {
            t_auth = status;
            dispatch_semaphore_signal(t_sem);
        }];
        dispatch_semaphore_wait(t_sem, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));

        if (t_auth != SFSpeechRecognizerAuthorizationStatusAuthorized)
        {
            t_fail_reason = "startListening: speech recognition permission denied";
            s_recognizer = nil;
            return;
        }

        // ── 3b. Request microphone access ─────────────────────────────────────
        // Speech recognition and microphone are separate permission gates on
        // macOS.  AVAudioEngine silently fails (or throws) if microphone access
        // is NotDetermined or Denied when it tries to open the input node.
        // Explicitly request it here, before touching the engine, so the dialog
        // is presented synchronously and we know the outcome before proceeding.
        // (The AVCaptureDevice completion handler is on an arbitrary queue, not
        // s_speech_queue, so there is no deadlock risk.)
        AVAuthorizationStatus t_mic_status =
            [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];

        if (t_mic_status == AVAuthorizationStatusNotDetermined)
        {
            dispatch_semaphore_t t_mic_sem = dispatch_semaphore_create(0);
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                                    completionHandler:^(BOOL /*granted*/) {
                dispatch_semaphore_signal(t_mic_sem);
            }];
            dispatch_semaphore_wait(t_mic_sem,
                dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
            t_mic_status =
                [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
        }

        if (t_mic_status != AVAuthorizationStatusAuthorized)
        {
            t_fail_reason = "startListening: microphone permission denied";
            s_recognizer = nil;
            return;
        }

        // ── 4. Set up the audio engine ────────────────────────────────────────
        // AVAudioEngine may throw an NSException (not just return NO) when the
        // audio subsystem is not yet ready — most commonly right after the user
        // grants microphone permission for the first time. Wrap in @try/@catch
        // so we can fail cleanly instead of crashing.
        s_engine = [[AVAudioEngine alloc] init];

        NSError *t_err = nil;
        BOOL t_engine_started = NO;
        @try
        {
            t_engine_started = [s_engine startAndReturnError:&t_err];
        }
        @catch (NSException *t_ex)
        {
            // Audio subsystem not ready — will be retried on next startListening call.
            t_engine_started = NO;
        }

        if (!t_engine_started)
        {
            t_fail_reason = "startListening: audio engine failed to start — try calling startListening again";
            s_engine = nil;
            s_recognizer = nil;
            return;
        }

        // ── 5. Start the first recognition task ───────────────────────────────
        s_is_listening = true;
        _start_recognition_task();
        t_success = true;
    });

    if (!t_success)
    {
        const char *t_msg = t_fail_reason != nil
            ? t_fail_reason
            : "startListening: speech recognition unavailable or not authorised";
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(t_msg, t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
    }

    return t_success;
}

void MCPlatformStopListening()
{
    _ensure_queue();
    dispatch_sync(s_speech_queue, ^{
        if (!s_is_listening)
            return;
        s_is_listening = false;
        _stop_engine();
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
    // Convert the return-delimited list to an NSArray on the calling thread,
    // then swap it in on s_speech_queue.

    NSMutableArray *t_new_phrases = [NSMutableArray array];

    if (p_phrases != nil && !MCStringIsEmpty(p_phrases))
    {
        char *t_cstr = nil;
        if (MCStringConvertToCString(p_phrases, t_cstr))
        {
            NSString *t_ns = [NSString stringWithUTF8String:t_cstr];
            MCMemoryDeallocate(t_cstr);
            if (t_ns != nil)
            {
                NSArray *t_parts = [t_ns componentsSeparatedByString:@"\n"];
                for (NSString *t_p in t_parts)
                {
                    NSString *t_trimmed = [t_p stringByTrimmingCharactersInSet:
                                           [NSCharacterSet whitespaceCharacterSet]];
                    if (t_trimmed.length > 0)
                        [t_new_phrases addObject:t_trimmed];
                }
            }
        }
    }

    _ensure_queue();
    dispatch_sync(s_speech_queue, ^{
        s_phrases = t_new_phrases;

        // If already listening, restart the task with the updated phrase list.
        if (s_is_listening)
            _start_recognition_task();
    });
}

void MCPlatformSetWakeWord(MCStringRef p_word, uint32_t p_timeout_ms)
{
    NSString *t_ns_word = nil;
    if (p_word != nil && !MCStringIsEmpty(p_word))
    {
        char *t_cstr = nil;
        if (MCStringConvertToCString(p_word, t_cstr))
        {
            t_ns_word = [NSString stringWithUTF8String:t_cstr];
            MCMemoryDeallocate(t_cstr);
        }
    }

    _ensure_queue();
    dispatch_sync(s_speech_queue, ^{
        s_wake_word       = t_ns_word;
        s_wake_timeout_ms = (p_timeout_ms > 0) ? p_timeout_ms : 5000;

        // Reset any active command window — setting the wake word while
        // listening takes effect immediately.
        _cancel_timer();

        if (s_is_listening)
            _start_recognition_task();
    });
}
