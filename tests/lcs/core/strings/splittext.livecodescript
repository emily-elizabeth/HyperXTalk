script "CoreStringsSplitText"
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

-- Joins the pieces with "|" so a whole result can be compared at once.
private function _Split pString, pPattern
   local tPieces, tResult, tIndex
   put splitText(pString, pPattern) into tPieces
   repeat with tIndex = 1 to the number of elements in tPieces
      put tPieces[tIndex] & "|" after tResult
   end repeat
   return the number of elements in tPieces & ":" & tResult
end _Split

on TestSplitTextBasic
   TestAssert "splitText returns an array", splitText("a,b", ",") is an array
   TestAssert "splits on a pattern", _Split("a, b;c", "\s*[,;]\s*") is "3:a|b|c|"
   TestAssert "no match gives the whole string", _Split("abc", ",") is "1:abc|"
   TestAssert "an empty string has no pieces", splitText(empty, ",") is empty
   TestAssert "runs of whitespace", _Split("one  two" & tab & "three", "\s+") is "3:one|two|three|"
   TestAssert "is case-sensitive", _Split("aXbxc", "x") is "2:aXb|c|"
end TestSplitTextBasic

on TestSplitTextEmptyPieces
   TestAssert "adjacent delimiters give an empty piece", _Split("a,,b", ",") is "3:a||b|"
   TestAssert "a leading delimiter gives an empty first piece", _Split(",a", ",") is "2:|a|"
   TestAssert "a trailing delimiter gives an empty last piece", _Split("a,", ",") is "2:a||"
end TestSplitTextEmptyPieces

on TestSplitTextEmptyMatches
   TestAssert "an empty pattern splits into characters", _Split("abc", empty) is "3:a|b|c|"
   TestAssert "empty matches give no empty pieces", _Split("baaac", "a*") is "2:b|c|"
   TestAssert "an empty match does not split a surrogate pair", \
         _Split("x" & numToCodepoint(0x1F600) & "y", empty) is "3:x|" & numToCodepoint(0x1F600) & "|y|"
   TestAssert "a lookaround can split without consuming", _Split("oneTwoThree", "(?=[A-Z])") is "3:one|Two|Three|"
end TestSplitTextEmptyMatches

on TestSplitTextContext
   TestAssert "^ only matches at the real start", _Split("aaa", "^a") is "2:|aa|"
   TestAssert "\b sees the preceding text", _Split("a cat concat", "\bcat\b") is "2:a | concat|"
end TestSplitTextContext

-- Returns the engine's message for the error thrown, or empty if nothing was.
private function _SplitTextError pString, pPattern
   local tError
   try
      get splitText(pString, pPattern)
   catch tError
      return line (item 1 of line 1 of tError) of the scriptExecutionErrors
   end try
   return empty
end _SplitTextError

on TestSplitTextErrors
   TestAssert "a bad pattern throws", _SplitTextError("abc", "(") contains "splitText: error in pattern"
   TestAssert "a runaway pattern throws instead of hanging", _SplitTextError("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", "(a+)+$") contains "splitText: pattern needs too many steps"
end TestSplitTextErrors
