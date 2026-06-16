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
// Cross-platform voice command support.
//
// Usage from HyperXTalk scripts:
//
//   registerVoiceCommand "open stack"
//   registerVoiceCommand "open stack" & return & "close stack" & return & "save stack"
//   -- Registers one or more voice command phrases. The phrase list may be a
//   -- single phrase or a return-delimited list of phrases.
//
//   unregisterVoiceCommand "open stack"
//   -- Removes a previously registered phrase. No-op if not registered.
//
//   unregisterAllVoiceCommands
//   -- Removes every registered voice command phrase.
//
//   startListening [language]
//   -- Begins listening for voice commands (and the wake word, if set).
//   -- language is an optional BCP-47 locale tag (e.g. "en-US", "fr-FR").
//   -- Defaults to the system locale if omitted.
//
//   stopListening
//   -- Stops listening immediately.
//
//   setWakeWord word [, timeoutMS]
//   -- Sets a wake word. After the wake word is detected, the engine listens
//   -- for a registered command for timeoutMS milliseconds (default: 5000).
//   -- If no command is recognised within that window, listenTimeoutExpired
//   -- is dispatched and the engine returns to wake-word-only mode.
//   -- Pass empty to clear the wake word (all recognised commands fire directly).
//
//   isListening()
//   -- Returns true if the engine is currently listening, false otherwise.
//
// Messages dispatched to the current card:
//
//   voiceCommand pPhrase
//   -- Fired when a registered phrase is recognised.
//
//   wakeWordDetected
//   -- Fired when the wake word is recognised (if one is set).
//
//   listenTimeoutExpired
//   -- Fired when the command window after a wake word expires without a match.
//

#ifndef SPEECH_H
#define SPEECH_H

#include "mcstring.h"

// ── Platform entry points (implemented in mac/w32/lnx-speech.*) ──────────────

// Start the speech recognition engine with the given BCP-47 language tag.
// Pass nil or empty to use the system default locale.
// Returns true on success; sets the result string and returns false on failure.
bool MCPlatformStartListening(MCStringRef p_language);

// Stop the speech recognition engine.
void MCPlatformStopListening();

// Returns true if the engine is currently active.
bool MCPlatformIsListening();

// Replace the full set of registered phrases on the platform side.
// Called whenever the phrase registry changes.
// p_phrases is a return-delimited list of phrases (may be empty string to clear).
void MCPlatformSetVoiceCommands(MCStringRef p_phrases);

// Set the wake word. Pass nil or empty to clear.
// p_timeout_ms is the command window in milliseconds (default 5000).
void MCPlatformSetWakeWord(MCStringRef p_word, uint32_t p_timeout_ms);

// ── Engine callbacks (called on the main thread by platform code) ─────────────

// Called when a registered phrase is matched.
void MCSpeechDispatchVoiceCommand(MCStringRef p_phrase);

// Called when the wake word is detected.
void MCSpeechDispatchWakeWordDetected();

// Called when the command window expires without a match.
void MCSpeechDispatchListenTimeoutExpired();

#endif // SPEECH_H
