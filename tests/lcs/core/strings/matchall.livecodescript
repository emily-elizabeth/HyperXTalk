script "CoreStringsMatchAll"
/*
Copyright (C) 2026 HyperXTalk contributors.

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

on TestMatchAllBasic
   local tMatches
   put matchAll("a1 b22 c333", "[a-z]([0-9]+)") into tMatches

   TestAssert "matchAll returns an array", tMatches is an array
   TestAssert "matchAll finds every match", the number of elements in tMatches is 3
   TestAssert "key 0 is the whole match", tMatches[1][0] is "a1" and tMatches[2][0] is "b22" and tMatches[3][0] is "c333"
   TestAssert "key 1 is the first group", tMatches[1][1] is "1" and tMatches[2][1] is "22" and tMatches[3][1] is "333"
end TestMatchAllBasic

on TestMatchAllNoMatch
   TestAssert "matchAll returns empty when nothing matches", matchAll("abc", "[0-9]") is empty
   TestAssert "matchAll on an empty string with a non-empty pattern", matchAll(empty, "a") is empty
end TestMatchAllNoMatch

on TestMatchAllContext
   local tMatches

   -- The text before each new starting point must stay visible to the pattern.
   put matchAll("aaa", "^a") into tMatches
   TestAssert "^ only matches at the real start of the string", the number of elements in tMatches is 1

   put matchAll("cat concat cat", "\bcat\b") into tMatches
   TestAssert "\b sees the preceding text", the number of elements in tMatches is 2

   put matchAll("xyxy zy", "(?<=x)y") into tMatches
   TestAssert "lookbehind sees the preceding text", the number of elements in tMatches is 2

   put matchAll("a" & return & "b" & return & "c", "(?m)^.") into tMatches
   TestAssert "(?m)^ matches at each line start", the number of elements in tMatches is 3 and tMatches[3][0] is "c"
end TestMatchAllContext

on TestMatchAllEmptyMatches
   local tMatches

   put matchAll("baaac", "a*") into tMatches
   TestAssert "empty matches advance by one character", the number of elements in tMatches is 4
   TestAssert "empty and non-empty matches interleave", tMatches[1][0] is empty and tMatches[2][0] is "aaa" and tMatches[3][0] is empty and tMatches[4][0] is empty

   put matchAll("abc", empty) into tMatches
   TestAssert "an empty pattern matches at every position", the number of elements in tMatches is 4

   put matchAll("x" & numToCodepoint(0x1F600) & "y", empty) into tMatches
   TestAssert "an empty match does not split a surrogate pair", the number of elements in tMatches is 4
end TestMatchAllEmptyMatches

on TestMatchAllGroups
   local tMatches

   put matchAll("B aB", "(a)?(B)") into tMatches
   TestAssert "optional group that took no part is empty", tMatches[1][1] is empty and tMatches[1][2] is "B"
   TestAssert "optional group that took part is filled", tMatches[2][1] is "a" and tMatches[2][2] is "B"
   TestAssert "every group has a key in every match", the number of elements in tMatches[1] is 3 and the number of elements in tMatches[2] is 3

   put matchAll("k1=v1;k2=v2", "(?<key>\w+)=(?<val>\w+)") into tMatches
   TestAssert "named groups are also keys", tMatches[1]["key"] is "k1" and tMatches[1]["val"] is "v1" and tMatches[2]["key"] is "k2" and tMatches[2]["val"] is "v2"
   TestAssert "named groups keep their numbers", tMatches[2][1] is "k2" and tMatches[2][2] is "v2"
end TestMatchAllGroups

on TestMatchAllCaseSensitive
   local tMatches
   put matchAll("aAa", "a") into tMatches
   TestAssert "matchAll is case-sensitive", the number of elements in tMatches is 2
   put matchAll("aAa", "(?i)a") into tMatches
   TestAssert "(?i) makes it case-insensitive", the number of elements in tMatches is 3
end TestMatchAllCaseSensitive

on TestMatchAllNative
   -- A native (non-Unicode) source string must give the same results.
   local tMatches
   put matchAll(numToChar(233) & "1" & numToChar(233) & "2", numToChar(233) & "(\d)") into tMatches
   TestAssert "native strings match", the number of elements in tMatches is 2 and tMatches[2][1] is "2"
end TestMatchAllNative

-- Returns the engine's message for the error thrown by matchAll, or empty
-- if nothing was thrown.
private function _MatchAllError pString, pPattern
   local tError
   try
      get matchAll(pString, pPattern)
   catch tError
      return line (item 1 of line 1 of tError) of the scriptExecutionErrors
   end try
   return empty
end _MatchAllError

on TestMatchAllErrors
   TestAssert "a bad pattern throws", _MatchAllError("abc", "(") contains "matchAll: error in pattern"
   TestAssert "a runaway pattern throws instead of hanging", _MatchAllError("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", "(a+)+$") contains "matchAll: pattern needs too many steps"
end TestMatchAllErrors
