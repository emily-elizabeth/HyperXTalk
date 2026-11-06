/* Copyright (C) 2003-2015 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "gtest/gtest.h"

#include "foundation.h"
#include "foundation-unicode.h"
#include "foundation-auto.h"
#include "foundation-bidi.h"

#include <string>

// Resolves the bidi levels of the given UTF-16 text and returns them as a
// string of digits (one per character) so that expectations are easy to read.
static std::string
resolve_levels(const unichar_t *p_chars, uindex_t p_char_count, intenum_t p_base_level)
{
	MCAutoStringRef t_string;
	if (!MCStringCreateWithChars(p_chars, p_char_count, &t_string))
		return "<alloc failed>";

	uint8_t *t_levels = nullptr;
	uindex_t t_level_count = 0;
	if (!MCBidiResolveTextDirection(*t_string, p_base_level, t_levels, t_level_count))
		return "<resolve failed>";

	std::string t_result;
	for (uindex_t i = 0; i < t_level_count; i++)
		t_result += char('0' + t_levels[i]);

	MCMemoryDeallocate(t_levels);
	return t_result;
}

#define RESOLVE(chars, base) \
	resolve_levels(chars, sizeof(chars) / sizeof(chars[0]), base)

TEST(bidi, unmatched_pdi_after_fsi)
//
// ">>> " FSI PDI PDI " <<<" used to pop the directional status stack past
// its bottom entry and crash the engine when pasted into a field.
//
{
	static const unichar_t t_text[] =
		{ '>', '>', '>', ' ', kMCBidiFSI, kMCBidiPDI, kMCBidiPDI, ' ', '<', '<', '<' };

	ASSERT_EQ("00000000000", RESOLVE(t_text, kMCTextDirectionAuto));
	ASSERT_EQ("00000000000", RESOLVE(t_text, kMCTextDirectionLTR));
	ASSERT_EQ("11111111111", RESOLVE(t_text, kMCTextDirectionRTL));
}

TEST(bidi, unmatched_pdi_without_initiator)
//
// A PDI with no isolate initiator at all must simply be ignored.
//
{
	static const unichar_t t_text[] = { 'a', kMCBidiPDI, 'b' };
	ASSERT_EQ("000", RESOLVE(t_text, kMCTextDirectionAuto));
}

TEST(bidi, excess_pdis_after_lri)
//
// Only the first PDI closes the isolate; the following ones are ignored.
//
{
	static const unichar_t t_text[] =
		{ kMCBidiLRI, kMCBidiPDI, kMCBidiPDI, kMCBidiPDI };
	ASSERT_EQ("0000", RESOLVE(t_text, kMCTextDirectionAuto));
}

TEST(bidi, nested_isolates_with_excess_pdi)
//
// LRI RLI x PDI PDI PDI: the levels step 0 -> 2 -> 4 and back down again,
// and the final unmatched PDI leaves the level at the base level.
//
{
	static const unichar_t t_text[] =
		{ kMCBidiLRI, kMCBidiRLI, 'x', kMCBidiPDI, kMCBidiPDI, kMCBidiPDI };
	ASSERT_EQ("024200", RESOLVE(t_text, kMCTextDirectionAuto));
}

TEST(bidi, sequential_isolates)
//
// Two balanced isolates in a row must each be opened and closed correctly.
//
{
	static const unichar_t t_text[] =
		{ kMCBidiLRI, 'a', kMCBidiPDI, kMCBidiLRI, 'b', kMCBidiPDI };
	ASSERT_EQ("020020", RESOLVE(t_text, kMCTextDirectionAuto));
}

TEST(bidi, pdi_restores_rtl_base_level)
//
// Closing an isolate inside an RTL paragraph must return to level 1, not 0.
//
{
	static const unichar_t t_text[] =
		{ 0x05D0, kMCBidiLRI, 'a', kMCBidiPDI, 0x05D1 };
	ASSERT_EQ("11211", RESOLVE(t_text, kMCTextDirectionAuto));
}

TEST(bidi, pdf_restores_rtl_base_level)
//
// In a forced RTL paragraph, text following a closed embedding must come
// back to the paragraph level (1) rather than 0. Previously the bottom of the
// directional status stack was always level 0, so "b" ended up at level 0.
//
{
	static const unichar_t t_text[] =
		{ 'a', kMCBidiRLE, 0x05D0, kMCBidiPDF, 'b' };
	ASSERT_EQ("23312", RESOLVE(t_text, kMCTextDirectionRTL));
}
