script "CoreStringsRegexUnicode"
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

-- Unicode matching is opt-in, through PCRE's (*UTF) and (*UCP) switches at
-- the start of a pattern. These tests pin down both the default and the
-- switched behaviour.

private function _Emoji
   return numToCodepoint(0x1F600)
end _Emoji

-- "cafe" with an acute e, two Greek letters, then x <emoji> y
private function _Sample
   return "caf" & numToCodepoint(0xE9) && numToCodepoint(0x3B1) & numToCodepoint(0x3B2) && "x" & _Emoji() & "y"
end _Sample

on TestRegexUnicodeDefault
   TestAssert "by default . sees an emoji as two code units", the number of elements in matchAll(_Emoji(), ".") is 2
   TestAssert "by default \w is ASCII only", the number of elements in matchAll(_Sample(), "\w+") is 3
   TestAssert "by default (?i) does not fold non-ASCII letters", not matchText(numToCodepoint(0xC9), "(?i)" & numToCodepoint(0xE9))
end TestRegexUnicodeDefault

on TestRegexUnicodeUTF
   local tMatches, tFound, tStart, tEnd
   put matchAll(_Emoji(), "(*UTF).") into tMatches
   TestAssert "(*UTF) makes . see an emoji as one character", the number of elements in tMatches is 1 and tMatches[1][0] is _Emoji()

   TestAssert "(*UTF) works in matchText", matchText("x" & _Emoji() & "y", "(*UTF)x(.)y", tFound) and tFound is _Emoji()
   TestAssert "(*UTF) works in matchChunk", matchChunk("x" & _Emoji() & "y", "(*UTF)x(.)y", tStart, tEnd) and tStart is 2 and tEnd is 2
   TestAssert "(*UTF) works in replaceText", replaceText("x" & _Emoji() & "y", "(*UTF).", "-") is "---"
   TestAssert "(*UTF) allows script property classes", the number of elements in matchAll(_Sample(), "(*UTF)\p{Greek}+") is 1
end TestRegexUnicodeUTF

on TestRegexUnicodeUCP
   local tMatches
   put matchAll(_Sample(), "(*UTF)(*UCP)\w+") into tMatches
   TestAssert "(*UCP) makes \w match non-ASCII letters", the number of elements in tMatches is 4
   TestAssert "... so an accented word stays whole", tMatches[1][0] is "caf" & numToCodepoint(0xE9)
   TestAssert "(*UCP) makes (?i) fold non-ASCII letters", matchText(numToCodepoint(0xC9), "(*UTF)(*UCP)(?i)" & numToCodepoint(0xE9))
   TestAssert "(*UCP) makes \b see non-ASCII letters", replaceText("caf" & numToCodepoint(0xE9) & "s", "(*UTF)(*UCP)\bcaf\b", "X") is "caf" & numToCodepoint(0xE9) & "s"
end TestRegexUnicodeUCP
