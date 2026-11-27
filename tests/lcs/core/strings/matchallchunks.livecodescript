script "CoreStringsMatchAllChunks"
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

-- Formats one group of one match as "start-end".
private function _Chunk pMatch, pGroup
   return pMatch[pGroup]["start"] & "-" & pMatch[pGroup]["end"]
end _Chunk

on TestMatchAllChunksBasic
   local tMatches, tText
   put "a1 b22 c333" into tText
   put matchAllChunks(tText, "[a-z]([0-9]+)") into tMatches

   TestAssert "matchAllChunks returns an array", tMatches is an array
   TestAssert "matchAllChunks finds every match", the number of elements in tMatches is 3
   TestAssert "key 0 is the range of the whole match", _Chunk(tMatches[1], 0) is "1-2" and _Chunk(tMatches[2], 0) is "4-6" and _Chunk(tMatches[3], 0) is "8-11"
   TestAssert "key 1 is the range of the first group", _Chunk(tMatches[1], 1) is "2-2" and _Chunk(tMatches[2], 1) is "5-6" and _Chunk(tMatches[3], 1) is "9-11"
   TestAssert "the ranges are usable as a char chunk", char tMatches[3][0]["start"] to tMatches[3][0]["end"] of tText is "c333"
   TestAssert "matchAllChunks returns empty when nothing matches", matchAllChunks("abc", "[0-9]") is empty
end TestMatchAllChunksBasic

on TestMatchAllChunksAgreesWithMatchChunk
   local tMatches, tStart, tEnd, tGroupStart, tGroupEnd
   get matchChunk("xx ab12 yy", "([a-z]+)([0-9]+)", tStart, tEnd, tGroupStart, tGroupEnd)
   put matchAllChunks("xx ab12 yy", "([a-z]+)([0-9]+)") into tMatches
   TestAssert "the first match has the positions matchChunk gives", \
         tMatches[1][1]["start"] is tStart and tMatches[1][1]["end"] is tEnd and \
         tMatches[1][2]["start"] is tGroupStart and tMatches[1][2]["end"] is tGroupEnd
end TestMatchAllChunksAgreesWithMatchChunk

on TestMatchAllChunksGroups
   local tMatches

   put matchAllChunks("B aB", "(a)?(B)") into tMatches
   TestAssert "a group that took no part has an empty start and end", \
         tMatches[1][1]["start"] is empty and tMatches[1][1]["end"] is empty and _Chunk(tMatches[1], 2) is "1-1"
   TestAssert "a group that took part has its range", _Chunk(tMatches[2], 1) is "3-3" and _Chunk(tMatches[2], 2) is "4-4"

   put matchAllChunks("k1=v1;k2=v2", "(?<key>\w+)=(?<val>\w+)") into tMatches
   TestAssert "named groups are also keys", _Chunk(tMatches[2], "key") is "7-8" and _Chunk(tMatches[2], "val") is "10-11"
end TestMatchAllChunksGroups

on TestMatchAllChunksEmptyMatches
   local tMatches
   put matchAllChunks("abc", "x*") into tMatches
   TestAssert "empty matches are found at every position", the number of elements in tMatches is 4
   TestAssert "an empty match ends one before it starts", _Chunk(tMatches[1], 0) is "1-0" and _Chunk(tMatches[4], 0) is "4-3"
end TestMatchAllChunksEmptyMatches

on TestMatchAllChunksCharacters
   -- Positions are in characters, not UTF-16 code units.
   local tMatches, tText
   put "a" & numToCodepoint(0x1F600) & "b1" into tText
   put matchAllChunks(tText, "b([0-9])") into tMatches
   TestAssert "a character outside the BMP counts as one", _Chunk(tMatches[1], 0) is "3-4" and _Chunk(tMatches[1], 1) is "4-4"
   TestAssert "... so the range selects the right text", char tMatches[1][0]["start"] to tMatches[1][0]["end"] of tText is "b1"
end TestMatchAllChunksCharacters

private function _MatchAllChunksError pString, pPattern
   local tError
   try
      get matchAllChunks(pString, pPattern)
   catch tError
      return the result
   end try
   return empty
end _MatchAllChunksError

on TestMatchAllChunksErrors
   TestAssert "a bad pattern throws and describes the problem in the result", _MatchAllChunksError("abc", "a(b") contains "missing"
end TestMatchAllChunksErrors
