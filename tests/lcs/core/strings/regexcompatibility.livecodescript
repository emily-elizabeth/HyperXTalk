script "CoreStringsRegexCompatibility"
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

-- Patterns that the regex library the engine used before (PCRE 8) accepted
-- and that its replacement (PCRE2) would reject if left to itself. The engine
-- keeps them working, with the meaning they had.

private function _Compiles pPattern
   try
      get matchText("abc", pPattern)
   catch tError
      return false
   end try
   return true
end _Compiles

on TestRegexCompatibilityUnknownEscape
   TestAssert "a backslash before an ordinary letter is that letter", matchText("jay", "^\jay$")
   TestAssert "... and does not match anything else", not matchText("kay", "^\jay$")
   TestAssert "... including inside a character class", matchText("y", "^[\y\i]$")
   TestAssert "escaped punctuation is still literal", matchText("a-b_c/d", "^a\-b\_c\/d$")
end TestRegexCompatibilityUnknownEscape

on TestRegexCompatibilityClassHyphen
   TestAssert "a hyphen after \w in a class is a literal hyphen", matchText("a-b.c", "^[\w-.]+$")
   TestAssert "... and the class is not otherwise widened", not matchText("a+b", "^[\w-.]+$")
   TestAssert "a hyphen after \d in a class is a literal hyphen", matchText("1-z", "^[\d-z]+$") and not matchText("y", "^[\d-z]+$")
   TestAssert "several such classes in one pattern", matchText("a-b@c-d", "^[\w-.]+@[\w-.]+$")
   TestAssert "a real range still works", matchText("m", "^[a-z]$") and not matchText("-", "^[a-z]$")
end TestRegexCompatibilityClassHyphen

on TestRegexCompatibilityStillErrors
   TestAssert "an unknown property is still an error", not _Compiles("\p{Foo}")
   TestAssert "... even beside an unknown escape", not _Compiles("\p{Foo}\j") 
   TestAssert "a range ending in a class escape is still an error", not _Compiles("[a-\d]")
   TestAssert "an unterminated group is still an error", not _Compiles("a(b")
end TestRegexCompatibilityStillErrors
