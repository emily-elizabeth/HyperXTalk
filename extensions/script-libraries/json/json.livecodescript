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

put JSONExport("hello, world") into tJSON
put JSONExport(42) into tJSON
put JSONExport(true) into tJSON
put JSONExport(false) into tJSON

local tHyperXTalkArray, tPrettyJSONArray
repeat with x=1 to 10
	put "number"&x into tHyperXTalkArray["label"&x]
end repeat
put JSONExport(tHyperXTalkArray) into tJSONArray

# prettyprint the output:
put JSONExport(tHyperXTalkArray, true) into tPrettyJSONArray

# prettyprint the output using 3 spaces instead of 4:
put JSONExport(tHyperXTalkArray, true, 3) into tPrettyJSONArray

put JSONExport(tHyperXTalkArray, true, 3, false) into tPrettyJSONArray

Description:

Exports HyperXTalk data as properly formatted JSON
Optionally prettyprinted with formatting options.

Parameters:

pValue (HyperXTalk):
A HyperXTalk object to export as a JSON object
string, number, array, boolean

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
JSON representation of the input HyperXTalk value
An error string is returned if the input value is invalid.
*/
function JSONExport pValue, pPretty, pHowManySpaces, pShouldExpandBraces
   local tJSON
   
   put lcb_JsonExport(pValue) into tJSON
   -- let the lcb library do the heavy lifting for speed
   -- but put the argument flexibility here because lcb can't do that
   if pPretty then
		if pHowManySpaces is empty then
			put 4 into pHowManySpaces
		end if
		if pShouldExpandBraces is empty then
			put true into pShouldExpandBraces
		end if
      return lcb_prettyJSON(tJSON, pHowManySpaces, pShouldExpandBraces)
   else
      return tJSON
   end if
end JSONExport

