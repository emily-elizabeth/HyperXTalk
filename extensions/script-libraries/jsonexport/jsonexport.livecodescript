script "com.hyperxtalk.library.jsonexport"

on extensionInitialize
   if the target is not me then
      pass extensionInitialize
   end if
   
   insert the script of me into back
   
   if the environment contains "development" then
      set the _ideoverride of me to true
   end if
end extensionInitialize

on extensionFinalize
   if the target is not me then
      pass extensionFinalize
   end if
   
   remove the script of me from back
end extensionFinalize

/**
Title: JSON Export Library

Version: 1.0.0

Author: HyperXTalk

Description: Export HyperXTalk values as JSON
*/

/**

Export a HyperXTalk value as JSON

Example:

put JsonExport("hello, world") into tJson
put JsonExport(42) into tJson
put JsonExport(true) into tJson
put JsonExport(false) into tJson

local tHyperXTalkArray, tPrettyJsonArray
repeat with x=1 to 10
	put "number"&x into tHyperXTalkArray["label"&x]
end repeat
put JsonExport(tHyperXTalkArray) into tJsonArray

# prettyprint the output:
put JsonExport(tHyperXTalkArray, true) into tPrettyJsonArray

# prettyprint the output using 3 spaces instead of 4:
put JsonExport(tHyperXTalkArray, true, 3) into tPrettyJsonArray

put JsonExport(tHyperXTalkArray, true, 3, false) into tPrettyJsonArray

Description:

Exports HyperXTalk data as properly formatted JSON
Optionally prettyprinted with formatting options.

Parameters:

pValue (HyperXTalk):
A HyperXTalk object to export as a JSON object
string, number, array, or boolean

pPretty (optional Boolean):
Prettyprint the resulting JSON object if True
Return unformatted JSON if False or empty

pHowManySpaces (optional Integer):
How many spaces to indent each level if pretty-printed.
The default is 4 spaces.

pShouldExpandBraces (optional Boolean):
If True, place braces and brackets on separate lines.
If False braces and brackets are at the end of the current line.
Default is True.

Returns:
JSON representation of the input HyperXTalk value.
An error string is returned if the input value is invalid.
*/
function JsonExport pValue, pPretty, pHowManySpaces, pShouldExpandBraces
   local tJson
   
   put lcb_JsonExport(pValue) into tJson
   -- let the lcb library do the heavy lifting for speed
   -- but put the argument flexibility here because lcb can't do that
   put _cleaningNonPrintableChars(tJson) into tJson
   if pPretty then
		if pHowManySpaces is empty then
			put 4 into pHowManySpaces
		end if
		if pShouldExpandBraces is empty then
			put true into pShouldExpandBraces
		end if
      return lcb_prettyJSON(tJson, pHowManySpaces, pShouldExpandBraces)
   else
      return tJson
   end if
end JsonExport

/**

Export a HyperXTalk value as JSON

Description:

JsonExportPretty is used to convert a HyperXTalk value into data encoded 
in JSON format. If pValue is of a type that cannot be converted to JSON, 
an error is thrown. Set pPretty to true for pretty-printed output, false 
to leave JSON intact.

Parameters:

pValue (HyperXTalk):
A HyperXTalk object to export as a JSON object
string, number, array, or boolean

pPretty (optional Boolean):
Prettyprint the resulting JSON object if True.
Return unformatted JSON if False or empty.

Returns:
JSON representation of the input HyperXTalk value.
An error string is returned if the input value is invalid.
*/
function JsonExportPretty pValue, pPretty
   return JsonExport(pValue, pPretty)
end JsonExportPretty

/*****************************************************************************
 * MARK: PhotonJSON
 *****************************************************************************/

/*
These two functions originated from the PhotonJSON library

Code repository: https://github.com/DTByte-Ltd/PhotonJSON
Web: https://dtbyte.com/
License: https://opensource.org/licenses/MIT
Date: Nov 2020
Last Update: October 19, 2025
Note: PhotonJSON follows JSON's standards presented
in https://www.json.org/json-en.html
*/

// non-printable characters to your unicode code (HEX)
constant kNonPrintableChars = "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,8216,8217,8220,8221,8212,8213,8230"
private function _cleaningNonPrintableChars pText
   set the formSensitive to false

   repeat for each item i in kNonPrintableChars
      if numToChar(i) is in pText then
         replace numToChar(i) with _convertAsciiToUnicodeNum(i) in pText
      end if
   end repeat

   return pText
end _cleaningNonPrintableChars

private function _convertAsciiToUnicodeNum pNum
   put baseConvert(pNum, 10, 16) into pNum
   switch the length of pNum
      case 1
         return "\u000" & pNum
         break
      case 2
         return "\u00" & pNum
         break
      case 3
         return "\u0" & pNum
         break
      case 4
         return "\u" & pNum
         break
   end switch
end _convertAsciiToUnicodeNum
