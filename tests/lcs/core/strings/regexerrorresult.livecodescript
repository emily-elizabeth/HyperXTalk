script "CoreStringsRegexErrorResult"
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

-- When a pattern does not compile, the regex functions throw and leave the
-- regex library's description of the problem in the result.

private function _ResultAfter pFunction, pPattern
   local tResult
   try
      switch pFunction
         case "matchText"
            get matchText("abc", pPattern)
            break
         case "matchChunk"
            get matchChunk("abc", pPattern)
            break
         case "matchAll"
            get matchAll("abc", pPattern)
            break
         case "replaceText"
            get replaceText("abc", pPattern, "x")
            break
         case "splitText"
            get splitText("abc", pPattern)
            break
      end switch
      return "no error"
   catch tError
      put the result into tResult
   end try
   return tResult
end _ResultAfter

on TestRegexErrorResult
   local tFunction
   repeat for each item tFunction in "matchText,matchChunk,matchAll,replaceText,splitText"
      TestAssert tFunction & " leaves a description of a bad pattern in the result", \
            _ResultAfter(tFunction, "a(b") contains "missing"
      TestAssert tFunction & " says where the problem is", \
            _ResultAfter(tFunction, "a(b") contains "at offset 3"
      TestAssert tFunction & " describes a different problem differently", \
            _ResultAfter(tFunction, "a[b") contains "]"
   end repeat
end TestRegexErrorResult
