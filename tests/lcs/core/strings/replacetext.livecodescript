script "CoreStringsReplaceText"
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

on TestReplaceTextBasic
   TestAssert "replaces a match", replaceText("malformed", "mal", "well") is "wellformed"
   TestAssert "replaces every match", replaceText("colour or color", "colou?r", "culler") is "culler or culler"
   TestAssert "no match leaves the string alone", replaceText("abc", "[0-9]", "x") is "abc"
   TestAssert "replacing with empty deletes", replaceText("a-b-c", "-", empty) is "abc"
   TestAssert "trailing whitespace", replaceText("name " & tab & " ", "[ \t]+$", empty) is "name"
   TestAssert "is case-sensitive", replaceText("aAa", "a", "x") is "xAx"
   TestAssert "(?i) makes it case-insensitive", replaceText("aAa", "(?i)a", "x") is "xxx"
end TestReplaceTextBasic

on TestReplaceTextContext
   -- The text before each new starting point must stay visible to the pattern.
   TestAssert "^ only matches at the real start", replaceText("aaa", "^a", "b") is "baa"
   TestAssert "\b sees the preceding text", replaceText("cat concat cat", "\bcat\b", "dog") is "dog concat dog"
   TestAssert "lookbehind sees the preceding text", replaceText("xyxy zy", "(?<=x)y", "!") is "x!x! zy"
   TestAssert "(?m)^ matches at each line start", replaceText("a" & return & "b", "(?m)^", "> ") is "> a" & return & "> b"
end TestReplaceTextContext

on TestReplaceTextEmptyMatches
   TestAssert "empty matches are replaced at every position", replaceText("abc", "x*", "-") is "-a-b-c-"
   TestAssert "empty and non-empty matches interleave", replaceText("baaac", "a*", "-") is "-b--c-"
   TestAssert "an empty source can match", replaceText(empty, "^$", "x") is "x"
   TestAssert "an empty source with no match", replaceText(empty, "a", "x") is empty
   TestAssert "an empty match does not split a surrogate pair", \
         replaceText("x" & numToCodepoint(0x1F600) & "y", empty, "-") is "-x-" & numToCodepoint(0x1F600) & "-y-"
end TestReplaceTextEmptyMatches

on TestReplaceTextReferences
   TestAssert "$n refers to a group", replaceText("John Smith", "(\w+) (\w+)", "$2, $1") is "Smith, John"
   TestAssert "\n refers to a group", replaceText("John Smith", "(\w+) (\w+)", "\2, \1") is "Smith, John"
   TestAssert "$0 is the whole match", replaceText("abc", "b", "[$0]") is "a[b]c"
   TestAssert "references apply to every match", replaceText("a1 b2", "([a-z])([0-9])", "$2$1") is "1a 2b"
   TestAssert "${n} separates a reference from a digit", replaceText("a1", "([a-z])([0-9])", "${2}0$1") is "10a"
   TestAssert "${name} refers to a named group", replaceText("k=v", "(?<key>\w)=(?<val>\w)", "${val}=${key}") is "v=k"
   TestAssert "a group that took no part is empty", replaceText("B", "(a)?(B)", "[$1$2]") is "[B]"
   TestAssert "a second digit is literal when there is no such group", replaceText("ab", "(a)", "$10") is "a0b"
   TestAssert "references work in non-native text", \
         replaceText(numToCodepoint(0x3B1) & "1", "(.)(\d)", "$2$1") is "1" & numToCodepoint(0x3B1)
end TestReplaceTextReferences

on TestReplaceTextLiterals
   TestAssert "$n with no such group is literal", replaceText("price", "price", "$5") is "$5"
   TestAssert "$n beyond the pattern's groups is literal", replaceText("ab", "(a)", "$2") is "$2b"
   TestAssert "${name} with no such name is literal", replaceText("x", "x", "${nope}") is "${nope}"
   TestAssert "\$ is a literal dollar", replaceText("ab", "(a)", "\$1") is "$1b"
   TestAssert "\\ is a literal backslash", replaceText("ab", "(a)", "\\1") is "\1b"
   TestAssert "a lone $ is literal", replaceText("a", "a", "$") is "$"
   TestAssert "a lone \ is literal", replaceText("a", "a", "\") is "\"
   TestAssert "other text after \ is literal", replaceText("a", "a", "C:\temp") is "C:\temp"
end TestReplaceTextLiterals

-- Returns the engine's message for the error thrown by replaceText, or empty
-- if nothing was thrown.
private function _ReplaceTextError pString, pPattern
   local tError
   try
      get replaceText(pString, pPattern, "x")
   catch tError
      return line (item 1 of line 1 of tError) of the scriptExecutionErrors
   end try
   return empty
end _ReplaceTextError

on TestReplaceTextErrors
   TestAssert "a bad pattern throws", _ReplaceTextError("abc", "(") contains "replaceText: bad parameter"
   TestAssert "a runaway pattern throws instead of hanging", _ReplaceTextError("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", "(a+)+$") contains "replaceText: pattern needs too many steps"
end TestReplaceTextErrors
