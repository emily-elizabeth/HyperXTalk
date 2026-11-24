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

//
// regex code for match functions
//

#ifndef	MCREGEX_H
#define	MCREGEX_H

#define REG_OKAY 0

#define PATTERN_CACHE_SIZE 20

//regex structure
typedef struct
{
	void *re_pcre;
	size_t re_nsub;
	size_t re_erroffset;
	// The regex library's code for a compile error, or 0.
	int re_errorcode;
	// JS-2013-07-01: [[ EnhancedFilter ]] The pattern associated with the compiled
	//   regexp (used by the cache).
	MCStringRef re_pattern;
	// JS-2013-07-01: [[ EnhancedFilter ]] The flags used to compile the pattern
	//   (used to implement caseSensitive option).
	int re_flags;
}
regex_t;

/* The structure in which a captured offset is returned. */

typedef int regoff_t;

typedef struct
{
	regoff_t rm_so;
	regoff_t rm_eo;
}
regmatch_t;

#define NSUBEXP  50

typedef struct _regexp
{
	regex_t *rexp;
	uint2 nsubs;
	regmatch_t matchinfo[NSUBEXP];
}
regexp;

// JS-2013-07-01: [[ EnhancedFilter ]] Updated to manage case and allow case-insensitive matching.
// MW-2013-07-01: [[ EnhancedFilter ]] Removed 'usecache' parameter as there's no reason not to use the cache.
regexp *MCR_compile(MCStringRef exp, bool casesensitive);
// Options for MCR_exec (p_options).
enum
{
	// An empty match at the starting offset is not acceptable.
	kMCRegexExecNotEmptyAtStart = 1 << 0,
	// The match must begin exactly at the starting offset.
	kMCRegexExecAnchored = 1 << 1,
};

// The outcome of MCR_exec (r_status).
enum
{
	kMCRegexExecMatched,
	kMCRegexExecNoMatch,
	// The match was abandoned because it needed too many steps.
	kMCRegexExecLimit,
	kMCRegexExecError,
};

// p_start is an offset within p_range at which to begin matching; the text
// before it stays visible to the pattern (for ^, \b and lookbehinds). If
// r_status is not nil it receives one of the kMCRegexExec* outcomes, which
// tells 'no match' apart from a failure.
int MCR_exec(regexp *prog, MCStringRef string, MCRange p_range, uindex_t p_start = 0, uint32_t p_options = 0, int *r_status = nil);

// Step through every match of prog in p_string, as a global search does.
// Start with x_offset = 0 and x_after_empty = false and call repeatedly,
// passing the same variables back in, until it returns false. After each
// true return the match is in prog->matchinfo (offsets from the start of
// p_string). An empty match is followed by an attempt at a non-empty match
// at the same place and then by a one character advance, so the search
// always terminates. r_status is as for MCR_exec.
bool MCR_exec_next(regexp *prog, MCStringRef p_string, uindex_t& x_offset, bool& x_after_empty, int& r_status);

// The number of entries of prog->matchinfo the pattern can fill: the whole
// match plus its capture groups, limited to NSUBEXP.
uindex_t MCR_groupcount(regexp *prog);

// Named capture groups. MCR_namecount returns how many names the pattern
// defines; MCR_copyname returns the p_index'th (0-based) name and the number
// of the group it refers to.
uindex_t MCR_namecount(regexp *prog);
bool MCR_copyname(regexp *prog, uindex_t p_index, uindex_t& r_group, MCNameRef& r_name);
void MCR_copyerror(MCStringRef &r_error);
void MCR_free(regex_t *prog);

// JS-2013-07-01: [[ EnhancedFilter ]] Clear out the PCRE cache.
void MCR_clearcache();

#endif
