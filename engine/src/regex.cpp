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

#include "prefix.h"

#include "globdefs.h"
#include "parsedef.h"
#include "regex.h"


// The regular expression library is PCRE2, built for 16-bit code units
// (PCRE2_CODE_UNIT_WIDTH=16 is defined by the build), which is the form
// MCStringRef exposes its Unicode contents in.
#include "pcre2.h"

/* Compile flags. */
#define REG_ICASE     0x01
#define REG_NEWLINE   0x02
/* Match flags. */
#define REG_NOTBOL    0x04
#define REG_NOTEOL    0x08
#define REG_NOTEMPTY_ATSTART 0x10
#define REG_ANCHORED         0x20
/* Not used, but kept for the callers that pass it. */
#define REG_EXTENDED  0

/* Status values returned by regcomp and regexec. */
enum
{
	REG_ASSERT = 1,  /* internal error */
	REG_BADPAT,      /* pattern error */
	REG_ESPACE,      /* failed to get memory */
	REG_INVARG,      /* bad argument */
	REG_NOMATCH,     /* match failed */
	REG_ELIMIT       /* match abandoned: too many steps */
};

static const char *pstring[] =
{
	"",                       /* REG_OKAY   */
	"internal error",         /* REG_ASSERT */
	"pattern error",          /* REG_BADPAT */
	"failed to get memory",   /* REG_ESPACE */
	"bad argument",           /* REG_INVARG */
	"match failed",           /* REG_NOMATCH */
	"match limit exceeded"    /* REG_ELIMIT */
};

/* The most steps (internal calls of PCRE2's match function) one match may
take before it is abandoned, so that a pattern with runaway backtracking
gives up rather than running for an unbounded time. This is PCRE2's own
default, set here so that it does not depend on how the library was
configured. */
#define REGEX_MATCH_LIMIT 10000000

/* The most memory, in kibibytes, the backtracking of one match may use. */
#define REGEX_HEAP_LIMIT (256 * 1024)

/*************************************************
*         Shared PCRE2 contexts and store        *
*************************************************/

/* These are created on first use and kept for the life of the process. The
match data block has room for NSUBEXP pairs of offsets, which is all that a
regexp can hold. */

static pcre2_compile_context *s_lenient_compile_context = NULL;
static pcre2_match_context *s_match_context = NULL;
static pcre2_match_data *s_match_data = NULL;

static bool regex_initialize(void)
{
	if (s_lenient_compile_context == NULL)
	{
		s_lenient_compile_context = pcre2_compile_context_create(NULL);
		if (s_lenient_compile_context == NULL)
			return false;
		
		// See regcomp for when this is used.
		pcre2_set_compile_extra_options(s_lenient_compile_context, PCRE2_EXTRA_BAD_ESCAPE_IS_LITERAL);
	}
	
	if (s_match_context == NULL)
	{
		s_match_context = pcre2_match_context_create(NULL);
		if (s_match_context == NULL)
			return false;
		
		pcre2_set_match_limit(s_match_context, REGEX_MATCH_LIMIT);
		pcre2_set_heap_limit(s_match_context, REGEX_HEAP_LIMIT);
	}
	
	if (s_match_data == NULL)
	{
		s_match_data = pcre2_match_data_create(NSUBEXP, NULL);
		if (s_match_data == NULL)
			return false;
	}
	
	return true;
}

/*************************************************
*          Translate error code to string        *
*************************************************/

static void regerror(int errcode, MCStringRef &errbuf)
{
	const char *message;
	message = (errcode < 0 || errcode >= (int)(sizeof(pstring)/sizeof(char *)))
	          ? "unknown error code" : pstring[errcode];
	
	if (errbuf != nil)
		MCValueRelease(errbuf);
	/* UNCHECKED */ MCStringCreateWithNativeChars((const char_t *) message, strlen(message), errbuf);
}

/*************************************************
*           Free store held by a regex           *
*************************************************/

static void regfree(regex_t *preg)
{
	pcre2_code_free((pcre2_code *)preg->re_pcre);
}

/*************************************************
*            Compile a regular expression        *
*************************************************/

/*
Arguments:
  preg        points to a structure for recording the compiled expression
  pattern     the pattern to compile
  cflags      compilation flags
 
Returns:      0 on success
              a non-zero status on failure, in which case re_errorcode and
              re_erroffset hold PCRE2's error code and the offset in the
              pattern at which the error was found
*/

static int regcomp(regex_t *preg, MCStringRef pattern, int cflags)
{
	int errorcode = 0;
	PCRE2_SIZE erroffset = 0;
	uint32_t options = 0;
	
	preg->re_pcre = NULL;
	preg->re_nsub = 0;
	preg->re_errorcode = 0;
	preg->re_erroffset = 0;
	
	if (!regex_initialize())
		return REG_ESPACE;

	if ((cflags & REG_ICASE) != 0)
		options |= PCRE2_CASELESS;
	if ((cflags & REG_NEWLINE) != 0)
		options |= PCRE2_MULTILINE;

	// AL-2014-08-20: [[ Bug 13186 ]] Ensure pattern string doesn't get
	// permanently converted to UTF-16
	MCAutoStringRef t_temp;
	if (!MCStringMutableCopy(pattern, &t_temp))
		return REG_ESPACE;

	// PCRE 8.x, which the engine used before, accepted two things that PCRE2
	// rejects. So that patterns written for the old library still compile
	// and mean what they did, each is put right and the compile tried again:
	//
	//   - A backslash followed by a letter with no special meaning (such as
	//     \j) was taken as that letter. The compile is repeated with an
	//     option that allows this - but only after that particular error,
	//     as the option would otherwise hide mistakes the old library also
	//     reported, such as an unknown \p{...} property.
	//
	//   - In a character class, a hyphen after an escape such as \d or \w
	//     that cannot begin a range (as in [\w-.]) was taken as a literal
	//     hyphen. PCRE2 reports the position just after the hyphen, so the
	//     hyphen is escaped in the working copy of the pattern.
	//
	// If the pattern still fails, the error reported is the first one, whose
	// offset refers to the pattern as it was written.
	pcre2_compile_context *t_context = NULL;
	for (int t_attempt = 0; ; t_attempt++)
	{
		int t_errorcode = 0;
		PCRE2_SIZE t_erroffset = 0;
		preg->re_pcre = pcre2_compile((PCRE2_SPTR)MCStringGetCharPtr(*t_temp),
									  MCStringGetLength(*t_temp),
									  options,
									  &t_errorcode,
									  &t_erroffset,
									  t_context);
		if (preg->re_pcre != NULL)
			break;
		
		if (t_attempt == 0)
		{
			errorcode = t_errorcode;
			erroffset = t_erroffset;
		}
		
		if (t_attempt >= 32)
			break;
		
		if (t_errorcode == PCRE2_ERROR_UNKNOWN_ESCAPE && t_context == NULL)
		{
			t_context = s_lenient_compile_context;
			continue;
		}
		
		if (t_errorcode == PCRE2_ERROR_CLASS_INVALID_RANGE &&
			t_erroffset >= 1 && t_erroffset <= MCStringGetLength(*t_temp) &&
			MCStringGetCharAtIndex(*t_temp, (uindex_t)t_erroffset - 1) == '-' &&
			MCStringInsert(*t_temp, (uindex_t)t_erroffset - 1, MCSTR("\\")))
			continue;
		
		break;
	}

	if (preg->re_pcre == NULL)
	{
		preg->re_errorcode = errorcode;
		preg->re_erroffset = erroffset;
		return errorcode == PCRE2_ERROR_NOMEMORY ? REG_ESPACE : REG_BADPAT;
	}

	/* UNCHECKED */ preg->re_pattern = MCValueRetain(pattern);
	preg->re_flags = cflags;

	uint32_t t_capture_count = 0;
	if (pcre2_pattern_info((const pcre2_code *)preg->re_pcre,
						   PCRE2_INFO_CAPTURECOUNT,
						   &t_capture_count) != 0)
	{
		MCValueRelease(preg->re_pattern);
		pcre2_code_free((pcre2_code *)preg->re_pcre);
		preg->re_pcre = NULL;
		return REG_ASSERT;
	}
	preg->re_nsub = t_capture_count;
	
	return REG_OKAY;
}

/*************************************************
*              Match a regular expression        *
*************************************************/

/* The 'startoffset' argument is the code unit index within 'string' at which
matching begins. The whole of 'string' is still visible to the pattern, so
anchors, word boundaries and lookbehinds see the text before 'startoffset'.
Returned offsets are relative to the start of 'string'. Groups that took no
part in the match, and slots beyond the pattern's groups, are set to -1. */

static int regexec(regex_t *preg, const unichar_t *string, int len, int startoffset,
				   size_t nmatch, regmatch_t pmatch[], int eflags)
{
	int rc;
	uint32_t options = 0;

	if ((eflags & REG_NOTBOL) != 0)
		options |= PCRE2_NOTBOL;
	if ((eflags & REG_NOTEOL) != 0)
		options |= PCRE2_NOTEOL;
	if ((eflags & REG_NOTEMPTY_ATSTART) != 0)
		options |= PCRE2_NOTEMPTY_ATSTART;
	if ((eflags & REG_ANCHORED) != 0)
		options |= PCRE2_ANCHORED;

	if (!regex_initialize())
		return REG_ESPACE;
	
	if (nmatch > NSUBEXP)
		nmatch = NSUBEXP;

	rc = pcre2_match((const pcre2_code *)preg->re_pcre,
					 (PCRE2_SPTR)string,
					 (PCRE2_SIZE)len,
					 (PCRE2_SIZE)startoffset,
					 options,
					 s_match_data,
					 s_match_context);

	if (rc == 0)
		rc = (int)nmatch;    /* All captured slots were filled in */

	if (rc >= 0)
	{
		PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(s_match_data);
		size_t i;
		for (i = 0 ; i < (size_t)rc && i < nmatch ; i++)
		{
			if (ovector[i*2] == PCRE2_UNSET || ovector[i*2+1] == PCRE2_UNSET)
				pmatch[i].rm_so = pmatch[i].rm_eo = -1;
			else
			{
				pmatch[i].rm_so = (regoff_t)ovector[i*2];
				pmatch[i].rm_eo = (regoff_t)ovector[i*2+1];
			}
		}
		for (; i < nmatch; i++)
			pmatch[i].rm_so = pmatch[i].rm_eo = -1;
		return REG_OKAY;
	}

	switch(rc)
	{
	case PCRE2_ERROR_NOMATCH:
		return REG_NOMATCH;
	case PCRE2_ERROR_MATCHLIMIT:
	case PCRE2_ERROR_DEPTHLIMIT:
	case PCRE2_ERROR_HEAPLIMIT:
		return REG_ELIMIT;
	case PCRE2_ERROR_NOMEMORY:
		return REG_ESPACE;
	case PCRE2_ERROR_NULL:
	case PCRE2_ERROR_BADOPTION:
	case PCRE2_ERROR_BADMAGIC:
	case PCRE2_ERROR_BADMODE:
	case PCRE2_ERROR_BADOFFSET:
	case PCRE2_ERROR_BADUTFOFFSET:
		return REG_INVARG;
	default:
		return REG_ASSERT;
	}
}

static MCStringRef regexperror;

void MCR_copyerror(MCStringRef &r_error)
{
    if (regexperror == nil)
        r_error = MCValueRetain(kMCEmptyString);
    else
        r_error = MCValueRetain(regexperror);
}

regex_t *MCregexcache[PATTERN_CACHE_SIZE];

// JS-2013-07-01: [[ EnhancedFilter ]] Updated to support case-sensitivity and caching.
// MW-2013-07-01: [[ EnhancedFilter ]] Tweak to take 'const char *' and copy pattern as required.
// MW-2013-07-01: [[ EnhancedFilter ]] Removed 'usecache' parameter as there's
//   no reason not to use the cache.
regexp *MCR_compile(MCStringRef exp, bool casesensitive)
{
	Boolean found = False;
	regex_t *re = nil;
	int flags = REG_EXTENDED;
	if (!casesensitive)
		flags |= REG_ICASE;

	// Search the cache - keeping a note of any free slots in t_last_free.
	uint2 i;
	int t_last_free = -1;
	for (i = 0 ; i < PATTERN_CACHE_SIZE ; i++)
	{
		if (MCregexcache[i] == NULL)
		{
			t_last_free = i;
		}
		
		// Notice we only do a pointer comparison here. So the cache will
		// work if the same pattern valueref is used more than once, but
		// not if a pattern has a different valueref but same content.
		if (MCregexcache[i] &&
			exp == MCregexcache[i]->re_pattern &&
			flags == MCregexcache[i]->re_flags)
		{
			found = True;
			re = MCregexcache[i];
			break;
		}
	}
	
	// If the pattern isn't found with the given flags, then create a new one.
	if (re == nil)
	{
		/* UNCHECKED */ re = new(std::nothrow) regex_t;
		int status;
		status = regcomp(re, exp, flags);
		if (status != REG_OKAY)
		{
			// Report PCRE's own description of what is wrong with the pattern
			// and where, falling back on the general message for the code.
			PCRE2_UCHAR t_buffer[256];
			int t_buffer_length = 0;
			if (re->re_errorcode != 0)
				t_buffer_length = pcre2_get_error_message(re->re_errorcode, t_buffer, sizeof(t_buffer) / sizeof(t_buffer[0]));
			
			MCAutoStringRef t_description, t_message;
			if (t_buffer_length > 0 &&
				MCStringCreateWithChars((const unichar_t *)t_buffer, t_buffer_length, &t_description) &&
				MCStringFormat(&t_message, "%@ at offset %d", *t_description, (int)re->re_erroffset))
			{
				if (regexperror != nil)
					MCValueRelease(regexperror);
				regexperror = t_message . Take();
			}
			else
				regerror(status, regexperror);
			delete re;
			return(nil);
		}
	}
	
	// If the pattern is new, put it in the cache.
	if (!found)
	{
		// If we don't have a free slot, evict a randomish one.
		if (t_last_free == -1)
		{
			t_last_free = MCHashPointer(exp) % PATTERN_CACHE_SIZE;
			MCR_free(MCregexcache[t_last_free]);
		}
		
		// Make sure the new re is at position 0 in the cache.
		// (We assume that if a pattern is used once then it is
		// likely to be used again immediately after).
		MCregexcache[t_last_free] = MCregexcache[0];
		MCregexcache[0] = re;
	}
	
	regexp *treg = nil;
	/* UNCHECKED */ treg = new(std::nothrow) regexp;
	treg->rexp = re;
	
	return treg;
}

// p_start is the code unit index, relative to the start of p_range, at which
// matching begins. Text in p_range before p_start remains visible to anchors,
// word boundaries and lookbehinds. The offsets stored in prog->matchinfo are
// relative to the start of p_range.
int MCR_exec(regexp *prog, MCStringRef string, MCRange p_range, uindex_t p_start, uint32_t p_options, int *r_status)
{
	int status;
	int flags = 0;
	
	if ((p_options & kMCRegexExecNotEmptyAtStart) != 0)
		flags |= REG_NOTEMPTY_ATSTART;
	if ((p_options & kMCRegexExecAnchored) != 0)
		flags |= REG_ANCHORED;
	
	if (r_status != nil)
		*r_status = kMCRegexExecError;
	
	// Make sure we take a copy of the string to ensure it is Unicode.
	// Otherwise, subsequent uses of 'string' will follow much slower
	// codepaths causing unexpected degredation in performance.
	MCAutoStringRef t_uni_string;
	if (!MCStringUnicodeCopy(string, &t_uni_string))
	{
		regerror(REG_ESPACE, regexperror);
		return 0;
	}
	
	// A start beyond the end of the range can never match.
	if (p_start > p_range . length)
	{
		if (r_status != nil)
			*r_status = kMCRegexExecNoMatch;
		return 0;
	}
	
	// AL-2014-06-25: [[ Bug 12676 ]] Ensure string is not unnativized by MCR_exec
	// AL-2015-02-05: [[ Bug 14504 ]] Now that 'CanBeNative' flag is preserved, we can just use MCStringGetCharPtr here.
	status = regexec(prog->rexp,
					 MCStringGetCharPtr(*t_uni_string) + p_range . offset,
					 p_range . length,
					 (int)p_start,
					 NSUBEXP,
					 prog->matchinfo,
					 flags);

	if (status != REG_OKAY)
	{
		if (status == REG_NOMATCH)
		{
			if (r_status != nil)
				*r_status = kMCRegexExecNoMatch;
			return (0);
		}
		if (status == REG_ELIMIT && r_status != nil)
			*r_status = kMCRegexExecLimit;
		//MCValueRelease(regexperror);
		regerror(status, regexperror);
		return(0);
	}
	if (r_status != nil)
		*r_status = kMCRegexExecMatched;
	return (1);
}

// Returns true if the pattern treats CR LF as a single newline, in which case
// a one character advance must not stop between the two.
static bool MCR_crlf_is_newline(regexp *prog)
{
	uint32_t t_newline = 0;
	if (pcre2_pattern_info((const pcre2_code *)prog->rexp->re_pcre, PCRE2_INFO_NEWLINE, &t_newline) != 0)
		return false;
	
	return t_newline == PCRE2_NEWLINE_ANY ||
			t_newline == PCRE2_NEWLINE_CRLF ||
			t_newline == PCRE2_NEWLINE_ANYCRLF;
}

bool MCR_exec_next(regexp *prog, MCStringRef p_string, uindex_t& x_offset, bool& x_after_empty, int& r_status)
{
	uindex_t t_length = MCStringGetLength(p_string);
	
	for (;;)
	{
		uint32_t t_options = 0;
		if (x_after_empty)
		{
			// The previous match was empty. Nothing more can be found after
			// an empty match at the very end; otherwise first look for a
			// non-empty match starting at the same place.
			if (x_offset >= t_length)
			{
				r_status = kMCRegexExecNoMatch;
				return false;
			}
			t_options = kMCRegexExecNotEmptyAtStart | kMCRegexExecAnchored;
		}
		
		if (MCR_exec(prog, p_string, MCRangeMake(0, t_length), x_offset, t_options, &r_status) != 0)
		{
			regoff_t t_start = prog->matchinfo[0].rm_so;
			regoff_t t_end = prog->matchinfo[0].rm_eo;
			
			// A match that ends before it starts (possible with \K inside a
			// lookaround), or before the place the search began, cannot be
			// stepped past safely.
			if (t_start < 0 || t_end < t_start || (uindex_t)t_end < x_offset)
			{
				r_status = kMCRegexExecError;
				return false;
			}
			
			x_offset = (uindex_t)t_end;
			x_after_empty = (t_start == t_end);
			return true;
		}
		
		// A failure, or no match from an ordinary search, ends the iteration.
		if (r_status != kMCRegexExecNoMatch || !x_after_empty)
			return false;
		
		// There was no non-empty match at the place of the last empty one, so
		// move on by one character - taking care not to stop inside a
		// surrogate pair or (when it counts as one newline) a CR LF - and
		// search normally from there.
		unichar_t t_char = MCStringGetCharAtIndex(p_string, x_offset);
		uindex_t t_advance = 1;
		if (x_offset + 1 < t_length)
		{
			unichar_t t_next = MCStringGetCharAtIndex(p_string, x_offset + 1);
			if (t_char >= 0xD800 && t_char <= 0xDBFF && t_next >= 0xDC00 && t_next <= 0xDFFF)
				t_advance = 2;
			else if (t_char == '\r' && t_next == '\n' && MCR_crlf_is_newline(prog))
				t_advance = 2;
		}
		x_offset += t_advance;
		x_after_empty = false;
	}
}

uindex_t MCR_groupcount(regexp *prog)
{
	size_t t_count = prog->rexp->re_nsub + 1;
	if (t_count > NSUBEXP)
		t_count = NSUBEXP;
	return (uindex_t)t_count;
}

uindex_t MCR_namecount(regexp *prog)
{
	uint32_t t_count = 0;
	if (pcre2_pattern_info((const pcre2_code *)prog->rexp->re_pcre, PCRE2_INFO_NAMECOUNT, &t_count) != 0)
		return 0;
	return (uindex_t)t_count;
}

bool MCR_copyname(regexp *prog, uindex_t p_index, uindex_t& r_group, MCNameRef& r_name)
{
	if (p_index >= MCR_namecount(prog))
		return false;
	
	// Each entry of the 16-bit name table is the group number in one code
	// unit followed by the zero-terminated name.
	uint32_t t_entry_size = 0;
	PCRE2_SPTR t_table = NULL;
	if (pcre2_pattern_info((const pcre2_code *)prog->rexp->re_pcre, PCRE2_INFO_NAMEENTRYSIZE, &t_entry_size) != 0 ||
		pcre2_pattern_info((const pcre2_code *)prog->rexp->re_pcre, PCRE2_INFO_NAMETABLE, &t_table) != 0 ||
		t_table == NULL || t_entry_size < 2)
		return false;
	
	PCRE2_SPTR t_entry = t_table + p_index * t_entry_size;
	
	uindex_t t_name_length = 0;
	while (t_name_length < (uindex_t)(t_entry_size - 1) && t_entry[1 + t_name_length] != 0)
		t_name_length++;
	
	MCAutoStringRef t_name;
	if (!MCStringCreateWithChars((const unichar_t *)(t_entry + 1), t_name_length, &t_name))
		return false;
	
	r_group = t_entry[0];
	return MCNameCreate(*t_name, r_name);
}

void MCR_free(regex_t *preg)
{
	if (preg)
	{
		regfree(preg);
		// MW-2013-07-01: [[ EnhancedFilter ]] Release the pattern.
		MCValueRelease(preg->re_pattern);
		delete preg;
	}
}

// JS-2013-07-01: [[ EnhancedFilter ]] Clear out the cache.
void MCR_clearcache()
{
	uint2 i;
	for (i = 0 ; i < PATTERN_CACHE_SIZE ; i++)
	{
		MCR_free(MCregexcache[i]);
		// PM-2014-10-02: [[ Bug 11647 ]] Make sure we clear old data to prevent a crash when restarting the app
		MCregexcache[i] = nil;
	}
}
