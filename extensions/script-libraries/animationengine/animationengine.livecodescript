script "com.hyperxtalk.library.animationengine"

on extensionInitialize
   if the target is me then
      insert the script of me into back
   end if
   
   if the environment contains "development" then
      set the _ideoverride of me to true
   end if
end extensionInitialize

on extensionFinalize
   if the target is me then
      remove the script of me from back
   end if
end extensionFinalize


----- Malte Brill AnimationEngine library ------

-- animationEngine 6.0.1
-- ©2005 - 2018 derbrill IT service. To get in touch contact info@derbrill.de
-- This stack was written by Malte Pfaff-Brill with the help of quite a few people
-- No rocket sience involved, however many books read any a lot of work went into
-- this stack. 

-- Licensing terms: animationEngine is set free into the Public Domain. The Software is provided "as is" with no warranty.
-- Availability of source code: you will find the latest sources and documentation on Github.

-- https://github.com/derbrill/animationEngine

-- Finally, thank you for helping out! If you purchased a license for animationEngine, I really appreciate that! 
-- It helped moving the library forward!

global animationEngineCurrentStackName

local sAELockMorph,sAEMorphing
local sAELockMorphGradientRamp,sAEMorphGradientRamp
local sAELockRotateGroup,sAERotateGroup
--> global stuff

-- on releaseStack
--   set the hilite of btn "useME" of me to false
-- end releaseStack

-- on libraryStack
--   if the short name of the target is not the short name of me then pass librarystack
--   if "dev" is in the environment and the version<5 then
--      answer "Sorry, this version of animationEngine requires a Livecode 5.0 or higher engine."
--      stop using me
--      exit librarystack
--   end if
--   aeResetFrameRate
-- end libraryStack

on aeWhatAmI
   put me&cr&"mode:"&&the mode of stack animationEngineCurrentStackName&cr&"style:"&&the style of stack animationEngineCurrentStackName&cr&"decorations:"&&the decorations of stack animationEngineCurrentStackName&cr&"path:"&&the effective filename of stack animationEngineCurrentStackName
end aeWhatAmI

--> constrain handlers

on touchStart pID
   try
      if the constrainRectangular of the target is not empty then
         set the uConstrainID of the target to pID
         send "constrainRectangularInit" to the target
      end if
      if the constrainElliptical of the target is not empty then
         set the uAllowConstrainDrag of the target to true
         set the uConstrainID of the target to pID
         send "constrainEllipticalInit"  to the target
      end if
      if the constrainCircular of the target is not empty then
         set the uAllowConstrainDrag of the target to true
         set the uConstrainID of the target to pID
         
         send "constrainCircularInit"  to the target
      end if
      if the constrainLinear of the target is not empty then
         set the uAllowConstrainDrag of the target to the mouseloc
         set the uConstrainID of the target to pID
         
         send "constrainLinearInit" to the target
      end if
      pass touchStart
   catch theError
      return "Error: "&theError
   end try
end touchStart

on mouseDown
   if the environment = "mobile" then pass mouseDown
   try
      if the constrainRectangular of the target is not empty then
         set the uAllowConstrainDrag of the target to the mouseh - item 1 of loc of target,the mousev - item 2 of loc of the target
         send "constrainRectangularInit" to the target
      end if
      if the constrainElliptical of the target is not empty then
         set the uAllowConstrainDrag of the target to true
         send "constrainEllipticalInit" to the target
      end if
      if the constrainCircular of the target is not empty then
         set the uAllowConstrainDrag of the target to true
         send "constrainCircularInit" to the target
      end if
      if the constrainLinear of the target is not empty then
         set the uAllowConstrainDrag of the target to the mouseloc
         send "constrainLinearInit" to the target
      end if
      pass mouseDown
   catch theError
      return "Error: "&theError
   end try
end mouseDown

on touchMove pID,x,y
   if the constrainRectangular of the target is not empty then
      if the uConstrainID of the target = pID and the uAllowConstrainDrag of the target is empty then
         set the uAllowConstrainDrag of the target to x - item 1 of loc of target, y - item 2 of loc of the target
      end if
      send "constrainRectangularInit" to the target
   end if
   if the uAllowConstrainDrag of the target is not empty then
      send "aeConstrainHandler" && x&","&y &"," &pID to the target
   end if
end touchMove

on mouseMove x,y
   if the environment = "mobile" then pass mouseMove
   if the uAllowConstrainDrag of the target is not empty then
      send "aeConstrainHandler" && x&","&y to the target
   end if
   pass mouseMove
end mouseMove

on aeConstrainHandler x,y,pID
   local allowDragOnLine,isAngle,radiusX,radiusY
   local theAngle,x1,x2,y1,TX,TY
   local y2
   if the constrainRectangular of the target is not empty then
      if the uAllowConstrainDrag of the target is empty then exit aeConstrainHandler
      if pID = the uConstrainID of the target then
         set the loc of the target to min(max(x - item 1 of the uAllowConstrainDrag of the target,item 1 of the constrainRectangular of the target+(the width of the target/2)),item 3 of the  constrainRectangular of the target - (the width of the target/2)),min(max(y - item 2 of the uAllowConstraindrag of the target,item 2 of the constrainRectangular of the target +(the height of the target/2)),item 4 of the constrainRectangular of the target-(the height of the target/2))
         send "constrainRectangularCallBack" to the target
      end if
   end if
   if the constrainElliptical of the target is not empty then
      if the uAllowConstrainDrag of the target is empty then exit aeConstrainHandler
      if pID = the uConstrainID of the target then
         put item 1 of the constrainElliptical of the target into tx
         put item 2 of the constrainElliptical of the target into ty
         put item 3 of the constrainElliptical of the target into radiusX
         put item 4 of the constrainElliptical of the target into radiusY
         put findAngleX(tx,ty,x,y) into isAngle
         put atan(radiusX/radiusY * tan (isAngle*pi/180))*180/pi into theAngle
         if isAngle > 90 and isAngle <= 270 then
            add 180 to theAngle
         end if
         set the loc of the target to pointOnEllipse(tx,ty,theAngle,radiusX,radiusY)
         send "constrainEllipticalCallBack" to the target
      end if
   end if
   if the constrainCircular of the target is not empty then
      if the uAllowConstrainDrag of the target is empty then exit aeConstrainHandler
      if pID = the uConstrainID of the target then
         put item 1 of the constrainCircular of the target into tX
         put item 2 of the constrainCircular of the target into tY
         put item 3 of the constrainCircular of the target into radiusX
         set the loc of the target to pointOnCircle(tx,ty,findangleX(tx,ty,x,y),radiusX)
         send "constrainCircularCallBack" to the target 
      end if
   end if
   if the constrainLinear of the target is not empty then
      if the uAllowConstrainDrag of the target is empty then exit aeConstrainHandler
      if pID = the uConstrainID of the target then
         
         if item 1 of the constrainLinear of the target<item 3 of the constrainLinear of the target then
            put item 1 of the constrainLinear of the target into x1
            put item 2 of the constrainLinear of the target into y1
            put item 3 of the constrainLinear of the target into x2
            put item 4 of the constrainLinear of the target into y2
         else
            put item 3 of the constrainLinear of the target into x1
            put item 4 of the constrainLinear of the target into y1
            put item 1 of the constrainLinear of the target into x2
            put item 2 of the constrainLinear of the target into y2
         end if 
         put closestpointOnLine(x1,y1,x2,y2,x,y) into allowDragOnLine
         put round(item 1 of allowDragOnLine) into item 1 of allowDragOnLine
         put round(item 2 of allowDragOnLine) into item 2 of allowDragOnLine
         
         if circleLineSegmentCollide(x1,y1,x2,y2,allowDragOnLine,1)=false then 
            if distance(x,y,x1,y1)<distance(x,y,x2,y2) then
               set the loc of the target to x1,y1
            else
               set the loc of the target to x2,y2
            end if
         else            
            set the loc of the target to allowDragOnLine
         end if
         send "constrainLinearCallBack" to the target
      end if
   end if
end aeConstrainHandler  

on mouseUp
   if the constrainRectangular of the target is not empty then
      set the uAllowConstrainDrag of the target to empty
      send "constrainRectangularExit" to the target
   end if
   if the constrainLinear of the target is not empty then
      set the uAllowConstrainDrag of the target to empty
      send "constrainLinearExit" to the target
   end if
   if the constrainCircular of the target is not empty then
      set the uAllowConstrainDrag of the target to empty
      send "constrainCircularExit" to the target
   end if
   if the constrainElliptical of the target is not empty then
      set the uAllowConstrainDrag of the target to empty
      send "constrainEllipticalExit" to the target
   end if
   pass mouseUp
end mouseUp

on mouseRelease
   send "aeDragRelease" to the target
   pass mouseRelease
end mouseRelease

on touchRelease
   if the uAllowConstrainDrag of the target is not empty then send "aeDragRelease" to the target
end touchRelease

on aeDragRelease
   if the constrainRectangular of the target is not empty then
      set the uAllowConstrainDrag of the target to empty
      set the uConstrainID of the target to empty
      
      send "constrainRectangularExit" to the target
   end if
   if the constrainLinear of the target is not empty then
      set the uAllowConstrainDrag of the target to empty
      set the uConstrainID of the target to empty
      
      send "constrainLinearExit" to the target
   end if
   if the constrainCircular of the target is not empty then
      set the uAllowConstrainDrag of the target to empty
      set the uConstrainID of the target to empty
      
      send "constrainCircularExit" to the target
   end if
   if the constrainElliptical of the target is not empty then
      set the uAllowConstrainDrag of the target to empty
      set the uConstrainID of the target to empty
      
      send "constrainEllipticalExit" to the target
   end if
end aeDragRelease

on touchEnd
   if the uAllowConstrainDrag of the target is not empty then send "aeDragRelease" to the target
end touchEnd

--> aeinfos

/**

Title: Animation Engine

Author: Malte Pfaff-Brill

Type: library

Version: 6.0.1

Description:
Animation Engine is a Script library providing geometry, collision
detection, easing functions, isometric and 3D projection utilities, and
general-purpose animation helpers.

*/

/**

Summary: Returns information about the Animation Engine library

Returns: The computed result
*/
function animationEngineInfo
   local animationEngineInfo
   try
      --assemble info text
      put "You are using animationEngine "&animationEngineVersion()&" " into animationEngineInfo
      put "built "&animationEngineDate()&" by derbrill Multimedia"&cr after animationEngineInfo
      put "type animationEngineInfo for details on usage" after animationEngineInfo
      return animationEngineInfo
   catch theError
      return "error: "&theError
   end try
end animationEngineInfo

/**

Summary: Returns the version number of the Animation Engine library

Returns: The computed result
*/
function animationEngineVersion
   try
      --which version is animationEngine?
      return the versionnumber of stack animationEngineCurrentStackName
   catch theError
      return "error: "&theError
   end try
end animationEngineVersion

/**

Summary: Returns the release date of the Animation Engine library

Returns: The computed result
*/
function animationEngineDate
   try
      --which creation date?
      return the CreationDate of stack animationEngineCurrentStackName
   catch theerror
      return "error: "&theError
   end try
end animationEngineDate

--> aspect ratio

/**

Summary: Calculates the aspect ratio of a width and height

Parameters:
isWidth (number): width
isHeight (number): height

Returns: The computed result
*/
function aspectRatio isWidth,isHeight
   try
      --return the width-to-height ratio
      if isWidth is not a number or isHeight is not a number then
         return "Error: Both parameters must be numbers!"
      end if
      return isWidth/isHeight
   catch theerror
      return "error: "&theError
   end try
end aspectRatio


-- toDo update to pass a control reference!!!
-- Der blöde Mist funktioniert nicht recht. 
-- Irgendwie ist der ganze aspectResize Kram nicht das was ich mir vorgestellt habe.
/**

Summary: Resizes dimensions while preserving the original aspect ratio

Returns: The computed result
*/
function aspectResize
   local i,isBottom,isLeft,isRectRatio
   local isRight,isTargetRatio,isTop,theValue
   try
      --assemble one string from parameters
      
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      --check syntax
      if the number of items of theValue<>4 then 
         return "Error: Syntax is aspectResize(left,top,right,bottom)"
         exit aspectResize
      end if
      
      --check syntax
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers"
            exit aspectResize
            exit repeat
         end if
      end repeat
      
      --split rectangle into 4 single values
      put item 1 of theValue into isLeft
      put item 2 of theValue into isTop
      put item 3 of theValue into isRight
      put item 4 of theValue into isBottom
      
      --if it isn´t already set
      --set the custom property holding the width-to-height ratio
      --to avoid rounding errors later
      if the aspectResize["ratio"] of the target is empty then
         set the aspectResize["ratio"] of the target to aspectRatio(the width of the target,the height of the target)
      end if
      
      --get width-to-height ratio of the target and the rectangle
      put the aspectResize["ratio"] of the target into isTargetRatio
      put aspectRatio(isRight-isLeft,isBottom-isTop) into isRectRatio
      
      --equal!
      if isTargetRatio=isRectRatio then
         return isRight-isLeft,isBottom-isTop
      else
         --not equal,target ratio is bigger/target is heigher
         if isRectRatio<isTargetRatio then
            return (isRight-isLeft),(isRight-isLeft)/isTargetRatio
         else
            --not equal, target ratio is smaller/target is broader
            return (isBottom-isTop)*istargetRatio,isbottom-isTop
         end if
      end if
   catch theerror
      return "error: "&theError
   end try
end aspectResize

--> collision detection

/**

Summary: Tests whether two circles overlap

Returns: The computed result
*/
function circleCollide
   local i,theValue,threshold,x1
   local x2,y1,y2
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      --check syntax
      if the number of items of theValue<>5 then 
         return "Error: Syntax is circleCollide(x1,y1,x2,y2,threshold)"
         exit circleCollide
      end if
      
      --check syntax
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit circleCollide
            exit repeat
         end if
      end repeat
      put item 1 of theValue into x1
      put item 2 of theValue into y1
      put item 3 of theValue into x2
      put item 4 of theValue into y2
      put item 5 of theValue into threshold
      if distance(x1,y1,x2,y2)<threshold then
         return true
      else
         return false
      end if
   catch theerror
      return "error: "&theError
   end try
end circleCollide

/**

Summary: Tests whether a circle and a line segment intersect

Returns: The computed result
*/
function circleLineSegmentCollide
   local i,intersectionPointX,intersectionPointY,radius
   local theValue,u,x1,x2
   local x3,y1,y2,y3
   try
      --assemble one string from parameters
      
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      --check syntax
      if the number of items of theValue<>7 then 
         return "Error: Syntax is circleLineSegmentCollide(x1,y1,x2,y2,x3,y3,radius)"
         exit circleLineSegmentCollide
      end if
      
      --check syntax
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit circleLineSegmentCollide
            exit repeat
         end if
      end repeat
      
      put item 1 of theValue into x1
      put item 2 of thevalue into y1
      put item 3 of theValue into x2
      put item 4 of thevalue into y2
      put item 5 of theValue into x3
      put item 6 of thevalue into y3
      put item 7 of theValue into radius
      
      put ((x3-x1)*(x2-x1)+(y3-y1)*(y2-y1))/((x2 - x1)^2 + (y2 - y1)^2) into u
      put x1+u*(x2-x1) into intersectionPointX
      put y1+u*(y2-y1) into intersectionPointY
      if u<1 and u>0 then
         if distance(intersectionPointX,intersectionPointY,x3,y3)<radius then
            return "true"
         else
            return "false"
         end if
      else
         if distance(x1,y1,x3,y3)>radius and distance(x2,y2,x3,y3)>radius then
            return "false"
         else 
            return "true"
         end if
      end if
   catch theerror
      return "error: "&theError
   end try
end circleLineSegmentCollide


--> closest points

/**

Summary: Returns the closest point on a line to a given point

Returns: The computed result
*/
function closestpointOnLine
   local closestPointX,closestPointY,i,theValue
   local u,x1,x2,x3
   local y1,y2,y3
   try
      --assemble one string from parameters
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      --check syntax
      if the number of items of theValue<>6 then 
         return "Error: Syntax is closestpointOnLine(x1,y1,x2,y2,x3,y3)"
         exit closestPointOnLine
      end if
      --check syntax
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit closestpointOnLine
            exit repeat
         end if
      end repeat
      
      put item 1 of theValue into x1
      put item 2 of thevalue into y1
      put item 3 of theValue into x2
      put item 4 of thevalue into y2
      put item 5 of theValue into x3
      put item 6 of thevalue into y3
      if ((x2 - x1)^2 + (y2 - y1)^2)<>0 then
         put ((x3-x1)*(x2-x1)+(y3-y1)*(y2-y1))/((x2 - x1)^2 + (y2 - y1)^2) into u
         put x1+u*(x2-x1) into closestPointX
         put y1+u*(y2-y1) into closestPointY
         return closestPointX,closestPointY
      else
         return x1,y1
      end if
   catch theerror
      return "error: "&theError
   end try
end closestPointOnLine

--> basic geometry

/**

Summary: Returns the slope of a line defined by two points

Returns: The computed result
*/
function getSlope 
   local i,theValue,xEnd,xStart
   local yEnd,yStart
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit getSlope
            exit repeat
         end if
      end repeat
      
      put item 1 of theValue into xStart
      put item 2 of theValue into yStart
      put item 3 of theValue into xEnd
      put item 4 of theValue into yEnd
      if xEnd-xStart<>0 then
         return (yEnd-yStart)/(xEnd-xStart)
      else 
         return "y"
      end if
   catch theerror
      return "error: "&theError
   end try
end getSlope

/**

Summary: Returns the distance between two points

Returns: The computed result
*/
function distance 
   -- Unglaublich wie viel code man schreibt nur um die Parameter zu checken. Wär chic das schlauer zu regeln...
   
   local adjacentLeg,hypotenuse,i,oppositeLeg
   local theValue,x1,x2,y1
   local y2
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit distance
            exit repeat
         end if
      end repeat
      
      put item 1 of theValue into x1
      put item 2 of theValue into y1
      put item 3 of theValue into x2
      put item 4 of theValue into y2
      if y2 is empty then
         return "Error: Syntax is distance(x1,y1,x2,y2)"&cr&theValue
      end if
      put x1-x2 into oppositeLeg
      put y1-y2 into adjacentLeg
      put round(sqrt (oppositeLeg^2+adjacentLeg^2)) into hypotenuse
      return hypotenuse
   catch theerror
      return "error: "&theError
   end try
end distance

/**

Summary: Returns the angle in degrees between two points

Returns: The computed result
*/
function findAngle
   local adjacentLeg,alpha,foundAngle,i
   local oppositeLeg,theValue,x1,x2
   local y1,y2
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit findAngle
            exit repeat
         end if
      end repeat
      
      put item 1 of theValue into x1
      put item 2 of theValue into y1
      put item 3 of theValue into x2
      put item 4 of theValue into y2
      put abs(x1-x2) into oppositeLeg
      put abs(y1-y2) into adjacentLeg
      if adjacentLeg<>0 then
         put atan(oppositeleg/adjacentLeg) into alpha 
         put alpha*180/pi into alpha
      else 
         put 90 into alpha
      end if
      switch
         case x1<=x2 and y1>=y2
            put alpha into foundAngle
            break
         case x1<=x2 and y1<=y2
            put 180-alpha into foundAngle
            break
         case x1>=x2 and y1<=y2
            put 180+alpha into foundAngle
            break
         case x1>=x2 and y1>=y2
            put 360-alpha into foundAngle
            break
      end switch
      return round(foundangle)
   catch theerror
      return "error: "&theError
   end try
end findAngle

/**

Summary: Returns a precise angle in degrees between two points

Returns: The computed result
*/
function findPreciseAngle
   --grrrrrr Das hätte ich mal von Anfang an so machen sollen... Solln die User doch selber runden.
   local adjacentLeg,alpha,foundAngle,i
   local oppositeLeg,theValue,x1,x2
   local y1,y2
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit findPreciseAngle
            exit repeat
         end if
      end repeat
      
      put item 1 of theValue into x1
      put item 2 of theValue into y1
      put item 3 of theValue into x2
      put item 4 of theValue into y2
      put abs(x1-x2) into oppositeLeg
      put abs(y1-y2) into adjacentLeg
      if adjacentLeg<>0 then
         put atan(oppositeleg/adjacentLeg) into alpha 
         put alpha*180/pi into alpha
      else 
         put 90 into alpha
      end if
      switch
         case x1<=x2 and y1>=y2
            put alpha into foundAngle
            break
         case x1<=x2 and y1<=y2
            put 180-alpha into foundAngle
            break
         case x1>=x2 and y1<=y2
            put 180+alpha into foundAngle
            break
         case x1>=x2 and y1>=y2
            put 360-alpha into foundAngle
            break
      end switch
      return foundangle
   catch theerror
      return "error: "&theError
   end try
end findPreciseAngle

/**

Summary: Returns a precise angle along the X axis between two points

Returns: The computed result
*/
function findPreciseAngleX
   local foundAngle,i,theValue
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit findPreciseAngleX
            exit repeat
         end if
      end repeat
      
      put findPreciseAngle(theValue)-90 into foundAngle
      if foundAngle<0 then put 360+foundAngle into foundAngle
      return foundangle
   catch theerror
      return "error: "&theError
   end try
end findPreciseAngleX

/**

Summary: Returns the angle along the X axis between two points

Returns: The computed result
*/
function findAngleX
   local foundAngle,i,theValue
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit findAngleX
            exit repeat
         end if
      end repeat
      
      put findAngle(theValue)-90 into foundAngle
      if foundAngle<0 then put 360+foundAngle into foundAngle
      return foundangle
   catch theerror
      return "error: "&theError
   end try
end findAngleX

/**

Summary: Returns the angle along the Y axis between two points

Returns: The computed result
*/
function findAngleY
   local foundAngle,i,theValue
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit findAngleY
            exit repeat
         end if
      end repeat
      
      put findAngle(theValue) into foundAngle
      return foundangle
   catch theerror
      return "error: "&theError
   end try
end findAngleY

/**

Summary: Returns the angle between three points

Returns: The computed result
*/
function findAngle2
   local foundAngle,i,theValue
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit findAngle2
            exit repeat
         end if
      end repeat
      -- fix in 2.0
      --put findAngle(theValue)-90+item 5 of theValue into foundAngle
      put findAngle(theValue)-90-item 5 of theValue into foundAngle
      -- end fix
      if foundAngle<0 then put 360+foundAngle into foundAngle
      if foundAngle>360 then put foundAngle mod 360 into foundAngle
      return foundangle
   catch theerror
      return "error: "&theError
   end try
end findAngle2

/**

Summary: Tests whether two rectangles intersect and returns the overlap

Returns: The computed result
*/
function intersectRect
   local bottom1,bottom2,i,left1
   local left2,right1,right2,s1
   local s3,theValue,top1,top2
   try
      --assemble one string from parameters
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      --check syntax: are there 8 items of theValue?
      if the number of items of theValue<>8 then 
         return "Error: Syntax is aspectResize(left1,top1,right1,bottom1,left2,top2,right2,bottom2)"
         exit intersectRect
      end if
      
      --check: syntax are all 8 items of theValue numbers?
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit intersectRect
            exit repeat
         end if
      end repeat
      
      --ok: Split theValue
      put item 1 of theValue into left1
      put item 2 of theValue into top1
      put item 3 of theValue into right1
      put item 4 of theValue into bottom1
      put item 5 of theValue into left2
      put item 6 of theValue into top2
      put item 7 of theValue into right2
      put item 8 of theValue into bottom2
      
      --rectangles dont intersect
      if left2>right1 then
         return false
      end if
      if right2<left1 then
         return false
      end if
      
      --they intersect... now calulate the new rectangle.
      --s1 is the topleft, s3 the botright
      
      --is rectangle 2 within rectangle 1?
      if left1<=left2 and right1>=right2 and top1<=top2 and bottom1>=bottom2 then
         put left2,top2 into s1
         put right2,bottom2 into s3
         return s1,s3
      end if
      
      --is rectangle 1 within rectangle 2?
      if left2<left1 and right2>right1 and top2<top1 and bottom2>bottom1 then
         put left1,top1 into s1
         put right1,bottom1 into s3
         return s1,s3
      end if
      
      --the following needs to be cleared up...
      if top1<=top2 and bottom1>=top2 then
         if right2<=right1 then
            if left1<left2 then
               put left2,top2 into s1
               put right2,bottom1 into s3
            else
               if bottom1<bottom2 then
                  put left1,top2 into s1
                  put right2,bottom1 into s3
               else
                  if top2<top1 then
                     put left2,top1 into s1
                     put right2,bottom2 into s3
                  else
                     put left1,top2 into s1
                     put right2,bottom2 into s3
                  end if
               end if
            end if
         else
            if left1<left2 then
               if bottom1<bottom2 then
                  put left2,top2 into s1
                  put right1,bottom1 into s3
               else
                  if top2>=top1 then 
                     put left2,top2 into s1
                     put right1,bottom2 into s3
                  else
                     put left2,top2 into s1
                     put right2,bottom1 into s3
                     return s1,s3
                  end if
               end if
            else
               if right2<right1 then
                  put left2,top1 into s1
                  put right1,bottom1 into s3
               else
                  if bottom1<=bottom2 then
                     put left1,top2 into s1
                     put right1,bottom1 into s3
                  else
                     put left1,top2 into s1
                     put right1,bottom2 into s3
                  end if
               end if
            end if
         end if
         return s1,s3
      end if
      
      if top1>top2 and bottom2>top1 then
         if right2<=right1 then
            if left1<left2 then
               if bottom2<bottom1 then
                  put left2,top1 into s1
                  put right2,bottom2 into s3
               else
                  put left2,top1 into s1
                  put right2,bottom1 into s3
               end if
            else
               if bottom1>bottom2 then
                  put left1,top1 into s1
                  put right2,bottom2 into s3
               else
                  put left1,top1 into s1
                  put right2,bottom1 into s3
               end if
            end if
         else
            if left1<left2 then
               if bottom1>bottom2 then
                  put left2,top1 into s1
                  put right1,bottom2 into s3
               else 
                  put left2,top1 into s1
                  put right1,bottom1 into s3
               end if
            else
               put left1,top1 into s1
               put right1,bottom2 into s3
            end if
         end if
         return s1,s3
      else
         return false
      end if
   catch theerror
      return "error: "&theError
   end try
end intersectRect

/**

Summary: Compares pixel masks between two objects at given points

Returns: The computed result
*/
function maskComparePoints
   local abc,b1,b2,bottom1
   local bottom2,e1,e2,i
   local left1,left2,right1,right2
   local s1,s2,t1,t2
   local theValue,top1,top2
   try
      --assemble one string from parameters
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      --check syntax: are there 8 items of theValue?
      if the number of items of theValue<>8 then 
         return "Error: Syntax is maskComparePoints(left1,top1,right1,bottom1,left2,top2,right2,bottom2)"
         exit maskComparePoints
      end if
      
      --check: syntax are all 8 items of theValue numbers?
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers"
            exit maskComparePoints
            exit repeat
         end if
      end repeat
      
      --ok: Split theValue
      put item 1 of theValue into left1
      put item 2 of theValue into top1
      put item 3 of theValue into right1
      put item 4 of theValue into bottom1
      put item 5 of theValue into left2
      put item 6 of theValue into top2
      put item 7 of theValue into right2
      put item 8 of theValue into bottom2
      
      
      put intersectRect(theValue) into abc
      if abc<>"false" then
         put abs(left1-item 1 of abc)+1 into s1
         put abs(left1-item 3 of abc)+1 into e1
         put abs(top1-item 2 of abc)+1 into t1
         put abs(top1-item 4 of abc)+1 into b1
         put abs(left2-item 1 of abc)+1 into s2
         put abs(left2-item 3 of abc)+1 into e2
         put abs(top2-item 2 of abc)+1 into t2
         put abs(top2-item 4 of abc)+1 into b2
         return s1,e1,t1,b1,s2,e2,t2,b2
      end if
   catch theerror
      return "error: "&theError
   end try
end maskComparePoints

/**

Summary: Returns the midpoint of the left edge of an object

Parameters:
theObject: The object to compute the midpoint for

Returns: The computed result
*/
function midLeft theObject
   local midPoint
   try
      put round(the bottom of theObject-the height of theObject/2) into midPoint
      return the left of theObject,midPoint
   catch theerror
      return "error: "&theError
   end try
end midleft

/**

Summary: Returns the midpoint of the right edge of an object

Parameters:
theObject: The object to compute the midpoint for

Returns: The computed result
*/
function midRight theObject
   local midPoint
   try
      put round(the bottom of theObject-the height of theObject/2) into midPoint
      return the right of theObject,midPoint
   catch theerror
      return "error: "&theError
   end try
end midRight

/**

Summary: Returns the midpoint of the top edge of an object

Parameters:
theObject: The object to compute the midpoint for

Returns: The computed result
*/
function midTop theObject
   local midPoint
   try
      put round(the right of theObject-the width of theObject/2) into midPoint
      return midPoint,the top of theObject
   catch theerror
      return "error: "&theError
   end try
end midTop

/**

Summary: Returns the midpoint of the bottom edge of an object

Parameters:
theObject: The object to compute the midpoint for

Returns: The computed result
*/
function midBottom theObject
   local midPoint
   try
      put round(the right of theObject-the width of theObject/2) into midPoint
      return midPoint,the bottom of theObject
   catch theerror
      return "error: "&theError
   end try
end midBottom

/**

Summary: Returns the alpha channel value of an object at a point

Parameters:
theObject: The object to compute the alpha channel value for

Returns: The computed result
*/
function splitAlpha theObject
   local i,tAlpha,theData,theLength
   local theLoops
   try
      put the alphadata of theObject into tAlpha
      replace numtochar(10) with numtochar(17) in tAlpha
      replace numtochar(13) with numtochar(17) in tAlpha
      put the number of chars of tAlpha div the width of theObject into theLoops
      put the width of theObject into theLength
      put tAlpha into theData
      repeat with i=theLoops down to 1
         put char theLength*i of theData&cr into char theLength*i of theData
      end repeat
      return theData
   catch theerror
      return "error: "&theError
   end try
end splitAlpha

-- completely obsolete with the new intersect fnction!
-- deprecate it
/**

Summary: Tests whether two image objects overlap using pixel comparison

Parameters:
object1: The first image object
object2: The second image object
threshold: The threshold value for pixel comparison

Returns: The computed result
*/
function imageCollide object1,object2,threshold
   try
      if threshold is empty then put 0 into threshold
      if imageCollide2(object1,object2,threshold)<>true then
         return false
      else
         return true
      end if
   catch theerror
      return "error: "&theError
   end try
end imageCollide

/**

Summary: Tests whether two image objects overlap using an alternate pixel method

Parameters:
object1: The first image object
object2: The second image object
threshold: The threshold value for pixel comparison

Returns: The computed result
*/
function imageCollide2 object1,object2,threshold
   local abc,counterChar,counterLine,endCounterChar
   local endCounterLine,offset1,offset2,offset3
   local offset4,theChar,theData1,theData2
   local theFirst,theLine,theSecond
   try
      if threshold is empty then put 0 into threshold
      if intersect(object1,object2,"bounds")="false" then return "false"
      put splitAlpha(object1) into theData1
      put splitAlpha(object2) into theData2
      put maskComparePoints(the rect of object1,the rect of object2) into abc
      if abc<>"false" then
         put item 3 of abc into offset1
         put item 1 of abc into offset2
         put item 7 of abc into offset3
         put item 5 of abc into offset4
         put item 4 of abc-item 3 of abc into endCounterLine
         put item 2 of abc-item 1 of abc into endCounterChar
         put -1 into counterLine
         repeat for each line theLine in thedata1
            add 1 to counterLine
            put -1 into counterChar
            repeat for each char theChar in line (offset1+counterline) of thedata1
               add 1 to counterChar
               if counterChar>endcounterchar then
                  exit repeat
               else
                  put chartonum(char(counterchar+offset2) of line (counterLine+offset1) of thedata1) into theFirst
                  put chartonum(char(counterchar+offset4) of line (counterLine+offset3) of thedata2) into theSecond
                  if thefirst+thesecond>255+threshold then
                     return "true"
                  end if
               end if
            end repeat
            if counterLine>endcounterline then
               return "false"
            end if
         end repeat
      end if
   catch theerror
      return "error: "&theError
   end try
end imageCollide2

/**

Summary: Returns the coordinates of a point on a circle at a given angle

Returns: The computed result
*/
function pointOnCircle
   local centerX,centerY,i,isAngle
   local isNewX,isNewY,isRadius,theValue
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit pointOnCircle
            exit repeat
         end if
      end repeat
      
      put item 1 of theValue into centerX
      put item 2 of theValue into centerY
      put item 3 of theValue into isAngle
      put item 4 of theValue into isRadius
      put centerX+cos(isAngle*pi/180)*isRadius into isNewX
      put centerY+sin(isAngle*pi/180)*isRadius into isNewY
      return round(isNewX),round(isNewY) 
   catch theerror
      return "error: "&theError
   end try
end pointOnCircle

/**

Summary: Returns the coordinates of a point on an ellipse at a given angle

Returns: The computed result
*/
function pointOnEllipse
   local centerX,centerY,ellipseNewX,ellipseNewY
   local i,isAngle,isRadiusX,isRadiusY
   local theValue
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit pointOnEllipse
            exit repeat
         end if
      end repeat
      
      put item 1 of theValue into centerX
      put item 2 of theValue into centerY
      put item 3 of theValue into isAngle
      put item 4 of theValue into isRadiusX
      put item 5 of theValue into isRadiusY
      --put atan((isRadiusY^2/isRadiusx^2)*tan(isangle/180*pi))*180/pi into isAngle
      
      put centerX+cos(isAngle*pi/180)*isRadiusX into ellipseNewX
      put centerY+sin(isAngle*pi/180)*isRadiusY into ellipseNewY
      return ellipseNewX,ellipseNewY
   catch theerror
      return "error: "&theError
   end try
end pointOnEllipse

/**

Summary: Returns the coordinates of a point on a line at a given distance

Returns: The computed result
*/
function pointOnLine
   local i,isDistance,myAngle,newX
   local newY,theValue,x1,x2
   local y1,y2
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit pointOnLine
            exit repeat
         end if
      end repeat
      
      put item 1 of theValue into x1
      put item 2 of theValue into y1
      put item 3 of theValue into x2
      put item 4 of theValue into y2
      put item 5 of theValue into isDistance
      put findAngle(x1,y1,x2,y2)-90 into myAngle
      put round(x1+cos(myangle*pi/180)*isDistance) into newX
      put round(y1+sin(myAngle*pi/180)*isdistance) into newY
      return newX,newY
   catch theerror
      return "error: "&theError
   end try
end pointOnLine


--Added in AE 1.5

/**

Summary: Returns the position of a point relative to a line

Returns: The computed result
*/
function whereIsThePoint
   local i,theParameters,tWhere,x1
   local x2,x3,y1,y2
   local y3
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theParameters
         else
            put param(i) after theParameters
         end if
      end repeat
      put item 1 of theParameters into x1
      put item 2 of theParameters into y1
      put item 3 of theParameters into x2
      put item 4 of theParameters into y2
      put item 5 of theParameters into x3
      put item 6 of theParameters into y3
      if x1 is not a number or y1 is not a number or x2 is not a number or y2 is not a number or x3 is not a number or y3 is not a number then
         return "Error: One of the parameters is not a point."
      end if
      put (x2 - x1)*(y3 - y1) - (x3 - x1)*(y2- y1) into tWhere
      switch 
         case tWhere<0 
            return "right"
            break
         case tWhere>0
            return "left"
            break
         default
            return "on"
            break
      end switch
   catch theError
      return "error: "&theError
   end try
end whereIsThePoint

/**

Summary: Tests whether a point lies inside a polygon

Returns: The computed result
*/
function pointInPoly
   local containsP,i,j,mycounter
   local pts,tot,x,y
   try
      if paramcount()<3 then
         if the number of items of param(1)<>2 then
            return "Error in parameters."&cr&"Syntax: pointInPoly(x,y,pointlist)"
         else
            put item 1 of param(1) into x
            put item 2 of param(1) into y
            put param(2) into pts 
         end if
      end if
      if paramcount()=3 then
         put param(1) into x
         put param(2) into y
         put param(3) into pts
      end if
      if paramcount()>3 then return "error: Syntax is pointInPoly(x,y,pointlist)"
      if x is not a number then return "Error in Parameters. X is not a number!"
      if y is not a number then return "Error in Parameters. y is not a number!"
      if the number of lines of pts<3 then return "error. Pointlist must contain at least 3 lines"
      put 0 into mycounter
      put the number of lines of pts into j
      put the number of lines of pts into tot
      put false into containsP
      repeat with i = 1 to tot
         if item 1 of line i of pts is not a number or item 2 of line i of pts is not a number then
            return "Error in pointlist."
         end if
         if item 1 of line j of pts is not a number or item 2 of line j of pts is not a number then
            return "Error in pointlist."
         end if
         if ((item 2 of line i of pts <= y and y<item 2 of line j of pts)) or ((item 2 of line j of pts <= y) and (y < item 2 of line i of pts)) then
            if x < ((item 1 of line j of pts - item 1 of line i of pts) * (y - item 2 of line i of pts) / (item 2 of line j of pts - item 2 of line i of pts)) + item 1 of line i of pts then
               put not containsP into containsP
            end if
         end if
         put i into j
      end repeat
      return containsP
   catch theError
      return "error: "&theError
   end try
end pointInPoly


/**

Summary: Returns the intersection point of two line segments

Returns: The computed result
*/
function lineSegmentIntersectionPoint 
   local ap1x,ap1y,ap2x,ap2y
   local ep1x,ep1y,ep2x,ep2y
   local i,m1,m2,t1
   local t2,thePoint,theValue,xrange1
   local xrange2,xs,yRange1,yRange2
   local ys
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then 
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      put item 1 of theValue into ap1x
      put item 2 of theValue into ap1y
      put item 3 of theValue into ep1x
      put item 4 of theValue into ep1y
      put item 5 of theValue into ap2x
      put item 6 of theValue into ap2y
      put item 7 of theValue into ep2x
      put item 8 of theValue into ep2y
      put empty into xs
      
      --Workaround funktioniert nicht...
      --Die zurückgegebene Steigung ist einfach zu groß.
      --Kannst Du mal versuchen deine Parallel zur y Achse version einzubauen?
      if ep1x is not ap1x and ep2x is not ap2x then
         put (ep1y-ap1y)/(ep1x-ap1x) into m1
         put (ep2y-ap2y)/(ep2x-ap2x) into m2 
         if m1 = m2 then return "no intersectionpoint"  
         put -(m1*ap1x-ap1y) into t1 
         put -(m2*ap2x-ap2y) into t2  
         put (t2-t1)/(m1-m2) into xs
         put m1 * xs + t1 into ys 
      else if ep1x is ap1x and ep2x is not ap2x then
         put (ep2y-ap2y)/(ep2x-ap2x) into m2 
         put -(m2*ap2x-ap2y) into t2  
         put ep1x into xs
         put m2 * xs + t2 into ys 
      else if ep1x is not ap1x and ep2x is ap2x then
         put (ep1y-ap1y)/(ep1x-ap1x) into m1
         put -(m1*ap1x-ap1y) into t1 
         put ep2x into xs
         put m1 * xs + t1 into ys     
         --put m1
      else if ep1x is ap1x and ep2x is ap2x and ep2x is ep1x then
         put ep2x into xs
         put  ep2y into ys
         --- ep2y ist einfach so gewählt, müsste man noch schaun was man da dann nimmt.
      end if
      --parallel lines have no intersection point
      --hier eventuell prüfen ob die ranges sich überlappen
      
      if m1 = m2 then 
         if getSlope(ap1x,ap1y,ep1x,ep1y)=getSlope(ap2x,ap2y,ep2x,ep2y) then
            return "no intersectionPoint"  
         else
            put closestpointOnLine(ap1x,ap1y,ep1x,ep1y,ap2x,ap2y) into thePoint
            put item 1 of thepoint into xs
            put item 2 of thePoint into ys
         end if
         
      end if
      --is the intersectionPoint on the lineSegment?
      put ap1x,ep1x into xrange1 
      put ap2x,ep2x into xrange2 
      put ap1y,ep1y into yRange1
      put ap2y,ep2y into yRange2
      if xs < min(xRange1) or xs < min(xRange2) or xs > max(xRange1) or xs > max(xRange2) then return "no intersectionPoint"
      if ys < min(yRange1) or ys < min(yRange2) or ys > max(yRange1) or ys > max(yRange2) then return "no intersectionPoint"
      --calculate y coordinate of intersectionpoint
      return xs, ys 
   catch theError
      return "error: "&theError
   end try
end lineSegmentIntersectionPoint

/**

Summary: Returns the intersection point of two infinite lines

Returns: The computed result
*/
function lineIntersectionPoint 
   local ap1x,ap1y,ap2x,ap2y
   local ep1x,ep1y,ep2x,ep2y
   local i,m1,m2,t1
   local t2,theValue,xs,ys
   try
      repeat with i=1 to paramcount()
         if i<paramcount() then 
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      
      put item 1 of theValue into ap1x
      put item 2 of theValue into ap1y
      put item 3 of theValue into ep1x
      put item 4 of theValue into ep1y
      put item 5 of theValue into ap2x
      put item 6 of theValue into ap2y
      put item 7 of theValue into ep2x
      put item 8 of theValue into ep2y
      put empty into xs
      
      --Workaround funktioniert nicht...
      --Die zurückgegebene Steigung ist einfach zu groß.
      --Kannst Du mal versuchen deine Parallel zur y Achse version einzubauen?
      if ep1x is not ap1x and ep2x is not ap2x then
         put (ep1y-ap1y)/(ep1x-ap1x) into m1
         put (ep2y-ap2y)/(ep2x-ap2x) into m2 
         if m1 = m2 then return "parallel"  
         put -(m1*ap1x-ap1y) into t1 
         put -(m2*ap2x-ap2y) into t2  
         put (t2-t1)/(m1-m2) into xs
         put m1 * xs + t1 into ys 
      else if ep1x is ap1x and ep2x is not ap2x then
         put (ep2y-ap2y)/(ep2x-ap2x) into m2 
         put -(m2*ap2x-ap2y) into t2  
         put ep1x into xs
         put m2 * xs + t2 into ys 
      else if ep1x is not ap1x and ep2x is ap2x then
         put (ep1y-ap1y)/(ep1x-ap1x) into m1
         put -(m1*ap1x-ap1y) into t1 
         put ep2x into xs
         put m1 * xs + t1 into ys     
         --put m1
      else if ep1x is ap1x and ep2x is ap2x and ep2x is ep1x then
         put ep2x into xs
         put  ep2y into ys
         --- ep2y ist einfach so gewählt, müsste man noch schaun was man da dann nimmt.
      end if
      --parallel lines have no intersection point
      --hier eventuell prüfen ob die ranges sich überlappen
      
      if m1 = m2 then 
         if getSlope(ap1x,ap1y,ep1x,ep1y)=getSlope(ap2x,ap2y,ep2x,ep2y) then
            return "parallel"  
         else
            return closestpointOnLine(ap1x,ap1y,ep1x,ep1y,ap2x,ap2y)
         end if
      end if
      return xs, ys 
   catch theError
      return "error: "&theError
   end try
end lineIntersectionPoint

/**

Summary: Tests whether two polygons intersect

Parameters:
pointlist1: The first polygon defined by a list of points
pointlist2: The second polygon defined by a list of points
method: The method to use for collision detection ("SAT", "pip", or default)

Returns: The computed result
*/
function polyCollide pointlist1,pointlist2,method
   local firstpoint,fourthpoint,linecounter,linecounter2
   local p1x,p1y,p2x,p2y
   local p3x,p3y,p4x,p4y
   local secondpoint,t,test1,test2
   local theLine,thirdpoint,x,xx
   local y,yy
   try
      put 0 into firstpoint
      repeat for each line secondpoint in pointlist1
         add 1 to firstpoint
         if firstpoint=1 then next repeat
         put item 1 of line (firstpoint-1) of pointlist1 into p1x
         put item 2 of line (firstpoint-1) of pointlist1 into p1y
         put item 1 of secondpoint into p2x
         put item 2 of secondpoint into p2y
         put 0 into thirdpoint
         repeat for each line fourthpoint in pointlist2
            add 1 to thirdpoint
            if thirdpoint=1 then next repeat
            put item 1 of line (thirdpoint-1) of pointlist2 into p3x
            put item 2 of line (thirdpoint-1) of pointlist2 into p3y
            put item 1 of fourthpoint into p4x
            put item 2 of fourthpoint into p4y
            put (p1y-p2y) into xx
            put (p2x-p1x) into yy
            repeat for each line t in pointlist1
               put item 1 of t*xx+item 2 of t*yy &comma after test1
            end repeat
            repeat for each line t in pointlist2
               put item 1 of t*xx+item 2 of t *yy &comma after test2
            end repeat
            if the last char of test1="," then delete char -1 of test1
            if the last char of test2="," then delete char -1 of test2
            if min(test2)>max(test1) or min(test1)>max(test2) then 
               return false
            end if
            put empty into test1
            put empty into test2
         end repeat
      end repeat
      
      switch method
         case "SAT" 
            return true
            break
            
         case "pip"
            repeat for each line theLine in pointlist1
               put item 1 of theLine into x
               put item 2 of theLine into y
               if pointInPoly(x,y,pointlist2) then return true
            end repeat
            repeat for each line theLine in pointlist2
               put item 1 of theLine into x
               put item 2 of theLine into y
               if pointInPoly(x,y,pointlist1) then return true
            end repeat
            return false
            break
            
         default
            put 0 into linecounter
            repeat for each line secondpoint in pointlist1
               add 1 to linecounter
               if linecounter=1 then next repeat
               put 0 into linecounter2
               repeat for each line fourthpoint in pointlist2
                  add 1 to linecounter2
                  if linecounter2=1 then next repeat
                  if item 1 of lineSegmentIntersectionPoint(line (linecounter-1) of pointlist1,secondpoint,line (linecounter2-1) of pointlist2,fourthpoint) is a number then
                     --put lineSegmentintersectionPoint(line counter-1 of pointlist1,secondpoint1,line counter2-1 of pointlist2,fourthpoint)into xyz
                     return true
                  end if
               end repeat
            end repeat
            return false
            break
      end switch
   catch theError
      return "error: "&theError
   end try
end polyCollide

--end added in 1.5

/**

Summary: Calculates a point on a Gielis superformula curve

Parameters:
m: The number of symmetries
n1: The first exponent
n2: The second exponent
n3: The third exponent
phi: The angle in radians
a: The scaling factor for the x-axis
b: The scaling factor for the y-axis

Returns: The computed result
*/
function superformula m,n1,n2,n3,phi,a,b
   local i,r,t1,t2
   local theValue,x,y
   try
      put m&","&n1&","&n2&","&n3&","&phi&","&a&","&b into theValue
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit superformula
            exit repeat
         end if
      end repeat
      
      put cos(m*phi/4)/a into t1
      put abs(t1) into t1
      put (t1)^n2 into t1
      
      put sin(m *phi/4)/b into t2
      put abs(t2) into t2
      put (t2)^n3 into t2
      
      put (t1+t2)^(1/n1) into r
      if abs(r)=0 then
         put 0 into x
         put 0 into y
      else
         put 1/r into r
         put r*cos(phi) into x
         put r*sin(phi) into y
      end if
      return round(x),round(y)
   catch theerror
      return "error: "&theError
   end try
end superformula

on animationEngineHelp topic
   -- Ist besser in dem Doku stack Aufgehoben
   -- Seit 5.0 im PDF
   try
      switch topic
         default
            put "There is help for the following topics:"&cr&the helpTopics of stack animationEngineCurrentStackName
      end switch
   catch theerror
      return "error: "&theError
   end try
end animationEngineHelp

on animationEngineInfo
   -- obsolete
   try
      set the loc of stack animationEngineCurrentStackName to the screenloc
      put "animationEngine v. "&the versionnumber of stack animationEngineCurrentStackName into fld "version" of stack animationEngineCurrentStackName
      toplevel me
      go cd 1 of stack animationEngineCurrentStackName
      show me
      set the decorations of stack animationEngineCurrentStackName to default
   catch theerror
      return "error: "&theError
   end try
end animationEngineInfo

on moveCircular 
   local cX,cY,isAngle,isRadius
   try
      --The handler creates a customPropertySet "moveCircular" for the target
      --It sets the following custom properties of the target in that customPropertySet
      --centerX is the X coordinate of the circle
      --centerY is the Y coordinate of the circle
      --isAngle is the center angle of the circle 
      --isRadius is the radius
      
      --If the custom properties are not set yet,
      --we need to initialize them first
      
      --default speed is 1
      
      if the moveCircular["step"] of the target is empty then
         set the moveCircular["step"] of the target to 1
      end if
      
      --default x-location is item 1 of the loc of the target 
      
      if the moveCircular["centerX"] of the target is empty then 
         set the moveCircular["centerX"] of the target to item 1 of the loc of the target
      end if
      
      --default y location is item 2 of the loc of the target
      
      if the moveCircular["centerY"] of the target is empty then
         set the moveCircular["centerY"] of the target to item 2 of the loc of the target
      end if
      
      --default angle is 360 degrees
      
      if the moveCircular["isAngle"] of the target is empty then 
         set the moveCircular["isAngle"] of the target to 360
      end if
      
      --default radius is 50 pixel
      
      if the moveCircular["isRadius"] of the target is empty then 
         set the moveCircular["isRadius"] of the target to 50
      end if
      
      --calculate the next angle
      
      set the moveCircular["isAngle"] of the target to the moveCircular["isAngle"]  of the target+the moveCircular["step"] of the target
      if the moveCircular["isAngle"] of the target>=360 or the moveCircular["isAngle"] of the target<0 then
         set the moveCircular["isAngle"] of the target to the moveCircular["isAngle"] of the target mod 360
      end if
      
      --now calculate the new location.
      
      put the moveCircular["centerX"] of the target into cX
      put the moveCircular["centerY"] of the target into cY
      put the moveCircular["isAngle"] of the target into isAngle
      put the moveCircular["isRadius"] of the target into isRadius
      set the loc of the target to pointOnCircle((cX,cY,isAngle,isRadius))
   catch theerror
      return "error: "&theError
   end try
end moveCircular

on moveCircularReversePath
   local newStep
   try
      put the moveCircular["step"] of the target into newStep
      put newStep*-1 into newStep
      set the moveCircular["step"] of the target to newStep
   catch theerror
      return "error: "&theError
   end try
end moveCircularReversePath


on moveElliptical
   local cX,cY,isAngle,isRadiusX
   local isRadiusY
   try
      --The handler creates a customPropertySet "moveElliptical" for the target
      --It sets the following custom properties of the target
      --cX is the X coordinate of the ellipse center
      --cY is the Y coordinate of the ellipse center
      --isAngle is the elipse center angle
      --radiusX is the radius in x-direction
      --radiusY is the radius in y-direction
      
      --if the custom properties of the target are not set yet,
      --we need to initialize them first
      
      --default speed is 1
      
      if the moveElliptical["step"] of the target is empty then
         set the moveElliptical["step"] of the target to 1
      end if
      
      --default x-location is item 1 of the location of the target
      
      if the moveElliptical["centerX"] of the target is empty then 
         set the moveElliptical["centerX"] of the target to item 1 of the loc of the target
      end if
      
      --default y-location is item 2 of the location of the target
      
      if the moveElliptical["centerY"] of the target is empty then
         set the moveElliptical["centerY"] of the target to item 2 of the loc of the target
      end if
      
      --default center angle is 360 degrees
      
      if the moveElliptical["isAngle"] of the target is empty then 
         set the moveElliptical["isAngle"] of the target to 360
      end if
      
      --default radiusX is 80 pixel
      
      if the moveElliptical["radiusX"] of the target is empty then 
         set the moveElliptical["radiusX"] of the target to 80
      end if
      
      --default radiusY is 50 pixel
      
      if the moveElliptical["radiusY"] of the target is empty then 
         set the moveElliptical["radiusY"] of the target to 50
      end if
      
      --calculate the next angle
      
      set the moveElliptical["isAngle"] of the target to the moveElliptical["isAngle"]  of the target+the moveElliptical["step"] of the target
      if the moveElliptical["isAngle"] of the target>=360 then
         set the moveElliptical["isAngle"] of the target to 0
      end if
      
      --now calculate the new location.
      
      put the moveElliptical["centerX"] of the target into cX
      put the moveElliptical["centerY"] of the target into cY
      put the moveElliptical["isAngle"] of the target into isAngle
      put the moveElliptical["radiusX"] of the target into isRadiusX
      put the moveElliptical["radiusY"] of the target into isRadiusY
      set the loc of the target to pointOnEllipse ((cx,cy,isAngle,isRadiusX,isRadiusY))
   catch theerror
      return "error: "&theError
   end try
end moveElliptical

on moveEllipticalReversePath
   local newStep
   try
      put the moveElliptical["step"] of the target into newStep
      put newStep*-1 into newStep
      set the moveElliptical["step"] of the target to newStep
   catch theerror
      return "error: "&theError
   end try
end moveEllipticalReversePath


on moveLinear
   try
      if the moveLinear["startPoint"] of the target is empty then
         set the moveLinear["startpoint"] of the target to 0,0
      end if
      if the moveLinear["endpoint"] of the target is empty then
         set the moveLinear["endpoint"] of the target to 100,100
      end if
      if the movelinear["isDistance"] of the target is empty then
         set the movelinear["isDistance"] of the target to 0
      end if
      if the movelinear["step"] of the target is empty then
         set the movelinear["step"] of the target to 1
      end if
      if the moveLinear["step"] of the target<0 then
         set the movelinear["step"] of the target to the movelinear["step"] of the target*-1
      end if
      set the moveLinear["isDistance"] of the target to the moveLinear["isDistance"] of the target+the moveLinear["step"] of the target
      if the moveLinear["isDistance"] of the target<distance(the moveLinear["startpoint"] of the target,the moveLinear["endpoint"] of the target) then
         set the loc of the target to (pointOnLine(the movelinear["startPoint"] of the target,the movelinear["endpoint"] of the target,the moveLinear["isDistance"] of the target))
      else
         set the loc of the target to (pointOnLine(the movelinear["startPoint"] of the target,the movelinear["endpoint"] of the target,distance(the moveLinear["startpoint"] of the target,the moveLinear["endpoint"] of the target)))
         set the moveLinear["isDistance"] of the target to 0
         set the movelinear["moveDone"] of the target to true
         send "moveLinearMoveDone" to the target
         if the movelinear["pingpong"] of the target is true then
            moveLinearReversePath
            set the moveLinear["moveDone"] of the target to false
         end if
      end if
   catch theerror
      return "error: "&theError
   end try
end moveLinear

on moveLinearMoveDone
   return the name of the target&&"completed it´s linear move"
end moveLinearMoveDone

on moveLinearReversePath
   local swapStartPointAndEndPoint
   try
      put the movelinear["endpoint"] of the target into swapStartPointAndEndPoint
      set the moveLinear["endpoint"] of the target to the moveLinear["startpoint"] of the target
      set the moveLinear["startpoint"] of the target to swapStartPointAndEndPoint
   catch theerror
      return "error: "&theError
   end try
end moveLinearReversePath

on movePolygonal
   local newCurr,oldDistance
   try
      if the movePolygonal["currentpoints"] of the target is empty then
         set the movePolygonal["currentpoints"] of the target to 0,0&cr&100,100&cr&100,0
      end if
      
      
      if the movePolygonal["current"] of the target is empty then
         set the movePolygonal["current"] of the target to 2
      end if
      
      if the movePolygonal["current"] of the target<2 then
         set the movePolygonal["current"] of the target to 2
      end if
      
      if the movePolygonal["current"] of the target>the number of lines of the movePolygonal["currentPoints"] of the target then
         put the movePolygonal["current"] of the target mod the number of lines of the movePolygonal["currentPoints"] of the target into newCurr
         if newCurr<2 then put 2 into newCurr
         if newCurr>the number of lines of the movePolyGonal["currentPoints"] of the target then put the number of lines of the movePolygonal["currentPoints"] of the target into newcurr
         set the movePolygonal["current"] of the target to newCurr 
      end if
      
      if the movePolygonal["isDistance"] of the target is empty then
         set the movePolygonal["isDistance"] of the target to 0
      end if
      
      if the movePolygonal["step"] of the target is empty then
         set the movePolygonal["step"] of the target to 1
      end if
      
      if the movePolygonal["step"] of the target<0 then
         set the movePolygonal["step"] of the target to the movePolygonal["step"] of the target*-1
      end if
      set the movePolygonal["startpoint"] of the target to line (the movepolygonal["current"] of the target-1) of the movePolygonal["currentPoints"] of the target
      set the movePolygonal["endPoint"] of the target to line (the movepolygonal["current"] of the target) of the movePolygonal["currentpoints"] of the target
      set the movePolygonal["isDistance"] of the target to the movePolygonal["isDistance"] of the target+the movePolygonal["step"] of the target
      if the movePolygonal["isDistance"] of the target<distance(the movePolygonal["startpoint"] of the target,the movePolygonal["endpoint"] of the target) then
         set the loc of the target to (pointOnLine(the movePolygonal["startPoint"] of the target,the movePolygonal["endpoint"] of the target,the movePolygonal["isDistance"] of the target))
      else
         put the movepolygonal["isDistance"] of the target into oldDistance
         repeat 
            --set the loc of the target to (the movePolygonal["endpoint"] of the target)
            put oldDistance-distance(the movePolygonal["startpoint"] of the target,the movePolygonal["endpoint"] of the target) into oldDistance
            if oldDistance>0 then
               --put oldDistance*-1 into olddistance
               set the movePolygonal["isDistance"] of the target to olddistance
               if the movePolyGonal["current"] of the target< the number of lines of the movePolygonal["currentPoints"] of the target then
                  set the movepolygonal["current"] of the target to the movePolygonal["current"] of the target+1
               else
                  set the movePolygonal["moveDone"] of the target to true
                  send "movePolygonalMoveDone" to the target
                  set the movePolygonal["current"] of the target to 2
                  exit movepolygonal
               end if
               set the movePolygonal["startpoint"] of the target to line (the movepolygonal["current"] of the target-1) of the movePolygonal["currentPoints"] of the target
               set the movePolygonal["endPoint"] of the target to line (the movepolygonal["current"] of the target) of the movePolygonal["currentpoints"] of the target
            else
               set the movePolygonal["startpoint"] of the target to line (the movepolygonal["current"] of the target-1) of the movePolygonal["currentpoints"] of the target
               set the movePolygonal["endPoint"] of the target to line (the movepolygonal["current"] of the target) of the movePolygonal["currentpoints"] of the target
               set the movepolyGonal["isDistance"] of the target to oldDistance+distance(the movePolygonal["startPoint"] of the target,the movePolygonal["endpoint"] of the target)
               set the loc of the target to pointOnLine(the movePolygonal["startPoint"] of the target,the movePolygonal["endpoint"] of the target,the movePolygonal["isDistance"] of the target)
               exit repeat
            end if
         end repeat
      end if
   catch theerror
      return "error: "&theError
   end try
end movePolygonal

on movePolygonalMoveDone
   return the name of the target&&"completed it´s polygonal move"
end movepolygonalMoveDone

on movePolygonalReversePath
   local newDist,testme,thepoints
   try
      put the movePolygonal["current"] of the target into testme
      put the movePolygonal["currentpoints"] of the target into thepoints
      set the movePolyGonal["currentpoints"] of the target to the movePolyGonal["reversedPoints"] of the target
      set the movepolygonal["reversedPoints"] of the target to thepoints
      set the movePolygonal["current"] of the target to (the number of lines of thePoints-testme)+2
      put the movePolyGonal["isDistance"] of the target into newDist
      set the movePolyGonal["isDistance"] of the target to distance(the movePolyGonal["startPoint"] of the target,the movePolygonal["endPoint"] of the target)-newdist
   catch theerror
      return "error: "&theError
   end try
end movePolygonalReversePath

on numbersOnly theKey
   local wasSelected
   put the selectedchunk
   try
      if word 1 of the long id of the target is "field" then
         switch
            case theKey is in "0123456789" 
               if word 2 of the selectedChunk>word 4 of the selectedchunk then
                  put theKey before char (word 2 of the selectedchunk) of the target
               else
                  put theKey into the selectedchunk
               end if
               break
            case theKey="."
               if "." is not in the text of the target then
                  if word 2 of the selectedChunk>word 4 of the selectedchunk then
                     put "." before char (word 2 of the selectedChunk) of the target
                  else
                     put "." into the selectedchunk
                  end if
               end if
               break
            case theKey="-"
               if "-" is not in the the text of the target then
                  put the selectedchunk of the target into wasSelected
                  put "-" before the target
                  select after char(word 2 of wasSelected) of the target
               else
                  put the selectedchunk of the target into wasSelected
                  delete the first char of the target
                  select before char(word 4 of wasSelected) of the target
               end if
               break
         end switch
      else
         return "The target must be a field!"
      end if
   catch theerror
      return "error: "&theError
   end try
end numbersOnly


setprop superShape theValue
   local a,b,i,loops
   local m,n1,n2,n3
   local oldloc,phi,theCallBack,thePoints
   try
      if word 1 of the long id of the target="graphic" then
         set the style of the long id of the target to "curve"
         
         if the number of items of theValue<7 then
            return "Error: syntax is set the supershape of grc "&quote&"yourGraphic"&quote&" to m,n1,n2,n3,xScale,yScale,loops"
            exit superShape
         end if
         
         put empty into thePoints
         put item 1 of theValue into m
         put item 2 of theValue into n1
         put item 3 of theValue into n2
         put item 4 of theValue into n3
         put item 5 of theValue into a
         put item 6 of theValue into b
         put item 7 of theValue into loops
         repeat with i=1 to loops 
            put i*((pi*2)/loops) into phi
            put phi*180/pi into phi
            put superformula(m,n1,n2,n3,phi,a,b)&cr after thePoints
            put loops-i into theCallBack
            send "superShapeCallBack theCallBack" to the target
         end repeat
         delete the last line of thepoints
         put the loc of the target into oldloc
         set the points of the target to thepoints
         set the loc of the target to oldloc
         pass supershape
      else
         return "Supershape only works for graphics"
         exit supershape
      end if
   catch theerror
      return "error: "&theError
   end try
end superShape

on superShapeCallBack
   
end superShapeCallBack

setprop superPath theValue
   local a,b,i,loops
   local m,n1,n2,n3
   local oldloc,phi,thePoints
   try
      if word 1 of the long id of the target="graphic" then
         set the style of the long id of the target to "curve"
         
         if the number of items of theValue<3 then
            return "Error: syntax is set the supershape of grc "&quote&"yourGraphic"&quote&" to m,xScale,yScale"
            exit superPath
         end if
         
         put empty into thePoints
         put item 1 of theValue into m
         put 1 into n1
         put 1 into n2
         put 1 into n3
         put item 2 of theValue into a
         put item 3 of theValue into b
         put 1400 into loops
         repeat with i=1 to loops 
            put i*((pi*2)/loops) into phi
            --put phi*180/pi into phi
            if the last line of thePoints<>superformula(m,n1,n2,n3,phi,a,b) then
               put superformula(m,n1,n2,n3,phi,a,b)&cr after thePoints
            end if
         end repeat
         put line 1 of thePoints&cr after thePoints
         delete the last char of thepoints
         
         --repeat with i=the number of lines of thePoints down to 2
         --if line i of thepoints=line i-1 of thepoints then delete line i of thepoints
         --end repeat
         put the loc of the target into oldloc
         set the points of the target to thepoints
         set the loc of the target to oldloc
         pass superPath
      else
         return "Supershape only works for graphics"
         exit superPath
      end if
   catch theerror
      return "error: "&theError
   end try
end superPath


setprop spiral theValue
   local accuracy,cx,cY,i
   local isWinding,myAngle,myRadius,pointList
   local Step,windings
   try
      if word 1 of the long id of the target="graphic" then
         set the style of the long id of the target to "curve"
         
         if the number of items of theValue<4 then
            return "Error: syntax is set the spiral of grc "&quote&"yourGraphic"&quote&" to cx,cy,step,windings,[high/low/draft]"
            exit spiral
         end if
         
         repeat with i=1 to 4
            if item i of theValue is not a number then
               return "Error: syntax is set the spiral of grc "&quote&"yourGraphic"&quote&" to cx,cy,step,windings"
               exit spiral
            end if
         end repeat
         
         if item 5 of theValue is a number then
            put item 5 of theValue into accuracy
            if item 5 of theValue<1 then
               put 1 into accuracy
            end if
            if item 5 of theValue>36 then
               put 36 into accuracy
            end if
         else
            switch item 5 of theValue
               case "high"
                  put 2 into accuracy
                  break
               case "low" 
                  put 4 into accuracy 
                  break
               case "draft" 
                  put 36 into accuracy
                  break
               default 
                  put 4 into accuracy
                  break
            end switch
         end if
         
         put item 1 of theValue into cx
         put item 2 of theValue into cY
         put item 3 of theValue into Step
         put item 4 of theValue into windings
         
         put 0 into isWinding
         put 0 into myAngle
         put 0 into myRadius
         
         repeat until isWinding>=windings
            add accuracy to myAngle
            add accuracy/360 to isWinding
            add step*accuracy to myRadius
            if myAngle>=360 then
               put myAngle-360 into myAngle
            end if
            put pointOnCircle (cx,cy,myAngle,myRadius)&cr after pointList
         end repeat
         set the points of the target to pointList
         pass spiral
      else
         return "Error: Target must be a graphic!"
         exit spiral
      end if
   catch theerror
      return "error: "&theError
   end try
end spiral

/*
-- Weg mit dem Mist. Was hat das überhaupt in einer animationsLib zu suchen???

getprop cell[which]
   local testMe,tRowNum,tColNum
   put the long id of the target into testMe
   try
      if "field" is not in the long id of the target then
         return "Error: Target must be a field!"
         exit cell
      end if
      put item 1 of which into tRownum
      put item 2 of which into tColnum
      set the itemdel to tab
      do "return item tColnum of line tRownum of "&testMe
   catch theerror
      return "error: "&theError
   end try
end cell

setprop cell[which] newValue
   local testMe,tRowNum,tColNum
   
   put the long id of the target into testMe
   try
      if "field" is not in the long id of the target then
         return "Error: Target must be a field!"
         exit cell
      end if
      put item 1 of which into tRownum
      put item 2 of which into tColnum
      set the itemdel to tab
      -- optionaly check here that newValue does not contain the itemdel:
      if the itemdel is in newValue then
         return "Error : new value should be only 1 item!"
         exit cell
      end if
      do "put "&newValue&" into item "&tColnum&" of line "&tRownum&" of "&testMe
   catch theerror
      return "error: "&theError
   end try
end cell
*/

setprop constrainLinear
   local which
   try
      if param(1)<>"" then
         repeat with i=1 to paramcount()
            if i<paramcount() then
               put param(i)&"," after which
            else
               put param(i) after which
            end if
         end repeat
         
         set the itemdel to ","
         if the number of items of which<>4 then
            return "Error: constrainLinear needs 4 parameters."
            exit constrainLinear
         end if
         repeat with i=1 to 4
            if item i of which is not a number then
               return "error: All parameters must be numbers"
               exit constrainLinear
            end if
         end repeat
      end if
      lock messages
      set the constrainRectangular of the target to empty
      set the constrainElliptical of the target to empty
      set the constrainCircular of the target to empty
      
      unlock messages
      pass constrainLinear
   catch theerror
      return "error: "&theError
   end try
end constrainLinear

setprop constrainCircular
   local i,which
   try
      if param(1)<>"" then
         repeat with i=1 to paramcount()
            if i<paramcount() then
               put param(i)&"," after which
            else
               put param(i) after which
            end if
         end repeat
         
         set the itemdel to ","
         if the number of items of which<>3 then
            return "Error: constrainCircular needs 3 parameters."
            exit constrainCircular
         end if
         repeat with i=1 to 3
            if item i of which is not a number then
               return "error: All parameters must be numbers"
               exit constrainCircular
            end if
         end repeat
      end if
      lock messages
      set the constrainRectangular of the target to empty
      set the constrainElliptical of the target to empty
      set the constrainLinear of the target to empty
      
      unlock messages
      pass constrainCircular
   catch theerror
      return "error: "&theError
   end try
end constrainCircular

setprop constrainRectangular
   local i,which
   try
      if param(1)<>"" then
         repeat with i=1 to paramcount()
            if i<paramcount() then
               put param(i)&"," after which
            else
               put param(i) after which
            end if
         end repeat
         set the itemdel to ","
         if the number of items of which<>4 then
            return "Error: constrainRectangular needs 4 parameters."
            exit constrainRectangular
         end if
         repeat with i=1 to 4
            if item i of which is not a number then
               return "Error: All parameters must be numbers"
               exit constrainRectangular
            end if
         end repeat
      end if
      lock messages
      set the constrainCircular of the target to empty
      set the constrainElliptical of the target to empty
      set the constrainLinear of the target to empty
      unlock messages
      pass constrainRectangular
   catch theerror
      return "error: "&theError
   end try
end constrainRectangular 

setprop constrainElliptical
   local i,which
   try
      if param(1)<>"" then
         repeat with i=1 to paramcount()
            if i<paramcount() then
               put param(i)&"," after which
            else
               put param(i) after which
            end if
         end repeat
         
         set the itemdel to ","
         if the number of items of which<>4 then
            return "Error: constrainElliptical needs 4 parameters."
            exit constrainElliptical
         end if
         repeat with i=1 to 4
            if item i of which is not a number then
               return "Error: All parameters must be numbers"
               exit constrainElliptical
            end if
         end repeat
      end if
      lock messages
      set the constrainCircular of the target to empty
      set the constrainRectangular of the target to empty
      set the constrainLinear of the target to empty
      unlock messages
      pass constrainElliptical
   catch theerror
      return "error: "&theError
   end try
end constrainElliptical

setprop aspectScale theValue
   try
      get aspectResize(item 1 to 4 of thevalue)
      set the width of the target to item 1 of it
      set the height of the target to item 2 of it
      set the lockloc of the target to false
      set the loc of the target to item 1 of thevalue+(item 3 of theValue-item 1 of theValue)/2,item 2 of theValue+(item 4 of theValue-item 2 of theValue)/2 
      if item 5 of theValue="true" then
         set the lockloc of the target to true
      end if
      if item 5 of theValue="false" then
         set the lockloc of the target to false
      end if
      pass aspectScale
   catch theerror
      return "error: "&theError
   end try
end aspectScale

setprop movePolyGonal[whichkey] theValue
   local myVal,counter,reversePoints
   try
      put the number of lines of theValue into myVal
      if whichKey="pointlist" then
         put -1 into counter
         repeat for each line theLine in theValue
            add 1 to counter
            put line (myVal-counter) of theValue&cr after reversePoints
         end repeat
         delete the last char of reversePoints
         set the movePolygonal["reversedPoints"] of the target to reversePoints
         set the movePolygonal["currentpoints"] of the target to theValue
         --beep
      end if
      pass movePolygonal
   catch theerror
      return "error: "&theError
   end try
end movePolyGonal






-- 2.0 Update
on constrainRectangularInit
end constrainRectangularInit

on constrainRectangularExit
end constrainRectangularExit

on constrainRectangularCallBack
end constrainRectangularCallBack

on constrainEllipticalInit
end constrainEllipticalInit

on constrainEllipticalExit
end constrainEllipticalExit

on constrainEllipticalCallBack
end constrainEllipticalCallBack

on constrainCircularInit
end constrainCircularInit

on constrainCircularExit
end constrainCircularExit

on constrainCircularCallBack
end constrainCircularCallBack

on constrainLinearInit
end constrainLinearInit

on constrainLinearExit
end constrainLinearExit

on constrainLinearCallBack
end constrainLinearCallBack

-- circular easing
on moveCircularEaseIn pNewValue,pDuration,pExponent
   try
      if pNewValue is not a number or pDuration is not a number then 
         return "error: Parameter is not a number"
         exit moveCircularEaseIn
      end if
      if pExponent is not a number and pExponent is not empty then
         return "error: Parameter is not a number"
         exit moveCircularEaseIn
      end if
      if pExponent is empty then put 2 into pExponent
      set the moveCircular["easeStart"] of the target to the moveCircular["step"] of the target
      set the moveCircular["easeEnd"] of the target to pNewValue
      set the moveCircular["easeDuration"] of the target to pDuration
      set the moveCircular["easeExponent"] of the target to pExponent
      set the moveCircular["easeStartTime"] of the target to the milliseconds
      send "moveCircularChangeStepIn" to the target
   catch theError
      return theError
   end try
end moveCircularEaseIn

on moveCircularChangeStepIn
   local tDuration,tElapsed,tEnd,tExponent
   local tMillisecs,tNewStep,tStart
   try
      put the moveCircular["easeStart"] of the target into tStart
      put the moveCircular["easeEnd"] of the target into tEnd
      put the moveCircular["easeDuration"] of the target into tDuration
      put the moveCircular["easeExponent"] of the target into tExponent
      put the moveCircular["easeStartTime"] of the target into tMillisecs
      put the milliseconds-tMillisecs into tElapsed
      put aeEaseIn(tStart,tEnd,tDuration,tElapsed,tExponent) into tNewStep
      set the moveCircular["step"] of the target to tNewStep
      if tNewStep<>tEnd then send "moveCircularChangeStepIn" to the target in 20 milliseconds
   catch theError
      return theError
   end try
end moveCircularChangeStepIn

on moveCircularEaseOut pNewValue,pDuration,pExponent
   try
      if pNewValue is not a number or pDuration is not a number then 
         return "error: Parameter is not a number"
         exit moveCircularEaseOut
      end if
      if pExponent is not a number and pExponent is not empty then
         return "error: Parameter is not a number"
         exit moveCircularEaseOut
      end if
      if pExponent is empty then put 2 into pExponent
      set the moveCircular["easeStart"] of the target to the moveCircular["step"] of the target
      set the moveCircular["easeEnd"] of the target to pNewValue
      set the moveCircular["easeDuration"] of the target to pDuration
      set the moveCircular["easeExponent"] of the target to pExponent
      set the moveCircular["easeStartTime"] of the target to the milliseconds
      send "moveCircularChangeStepOut" to the target
   catch theError
      return theError
   end try
end moveCircularEaseOut

on moveCircularChangeStepOut
   local tDuration,tElapsed,tEnd,tExponent
   local tMillisecs,tNewStep,tStart
   try
      put the moveCircular["easeStart"] of the target into tStart
      put the moveCircular["easeEnd"] of the target into tEnd
      put the moveCircular["easeDuration"] of the target into tDuration
      put the moveCircular["easeExponent"] of the target into tExponent
      put the moveCircular["easeStartTime"] of the target into tMillisecs
      put the milliseconds-tMillisecs into tElapsed
      put aeEaseOut(tStart,tEnd,tDuration,tElapsed,tExponent) into tNewStep
      set the moveCircular["step"] of the target to tNewStep
      if tNewStep<>tEnd then send "moveCircularChangeStepOut" to the target in 20 milliseconds
   catch theError
      return theError
   end try
end moveCircularChangeStepOut

on moveCircularEaseInOut pNewValue,pDuration,pExponent
   try
      if pNewValue is not a number or pDuration is not a number then 
         return "error: Parameter is not a number"
         exit moveCircularEaseInOut
      end if
      if pExponent is not a number and pExponent is not empty then
         return "error: Parameter is not a number"
         exit moveCircularEaseInOut
      end if
      if pExponent is empty then put 2 into pExponent
      set the moveCircular["easeStart"] of the target to the moveCircular["step"] of the target
      set the moveCircular["easeEnd"] of the target to pNewValue
      set the moveCircular["easeDuration"] of the target to pDuration
      set the moveCircular["easeExponent"] of the target to pExponent
      set the moveCircular["easeStartTime"] of the target to the milliseconds
      send "moveCircularChangeStepInOut" to the target
   catch theError
      return theError
   end try
end moveCircularEaseInOut

on moveCircularChangeStepInOut
   local tDuration,tElapsed,tEnd,tExponent
   local tMillisecs,tNewStep,tStart
   try
      put the moveCircular["easeStart"] of the target into tStart
      put the moveCircular["easeEnd"] of the target into tEnd
      put the moveCircular["easeDuration"] of the target into tDuration
      put the moveCircular["easeExponent"] of the target into tExponent
      put the moveCircular["easeStartTime"] of the target into tMillisecs
      put the milliseconds-tMillisecs into tElapsed
      put aeEaseInOut(tStart,tEnd,tDuration,tElapsed,tExponent) into tNewStep
      set the moveCircular["step"] of the target to tNewStep
      if tNewStep<>tEnd then send "moveCircularChangeStepInOut" to the target in 20 milliseconds
   catch theError
      return theError
   end try
end moveCircularChangeStepInOut

-- elliptical easing
on moveEllipticalEaseIn pNewValue,pDuration,pExponent
   try
      if pNewValue is not a number or pDuration is not a number then 
         return "error: Parameter is not a number"
         exit moveEllipticalEaseIn
      end if
      if pExponent is not a number and pExponent is not empty then
         return "error: Parameter is not a number"
         exit moveEllipticalEaseIn
      end if
      if pExponent is empty then put 2 into pExponent
      set the moveElliptical["easeStart"] of the target to the moveElliptical["step"] of the target
      set the moveElliptical["easeEnd"] of the target to pNewValue
      set the moveElliptical["easeDuration"] of the target to pDuration
      set the moveElliptical["easeExponent"] of the target to pExponent
      set the moveElliptical["easeStartTime"] of the target to the milliseconds
      send "moveEllipticalChangeStepIn" to the target
   catch theError
      return theError
   end try
end moveEllipticalEaseIn

on moveEllipticalChangeStepIn
   local tDuration,tElapsed,tEnd,tExponent
   local tMillisecs,tNewStep,tStart
   try
      put the moveElliptical["easeStart"] of the target into tStart
      put the moveElliptical["easeEnd"] of the target into tEnd
      put the moveElliptical["easeDuration"] of the target into tDuration
      put the moveElliptical["easeExponent"] of the target into tExponent
      put the moveElliptical["easeStartTime"] of the target into tMillisecs
      put the milliseconds-tMillisecs into tElapsed
      put aeEaseIn(tStart,tEnd,tDuration,tElapsed,tExponent) into tNewStep
      set the moveElliptical["step"] of the target to tNewStep
      if tNewStep<>tEnd then send "moveEllipticalChangeStepIn" to the target in 20 milliseconds
   catch theError
      return theError
   end try
end moveEllipticalChangeStepIn

on moveEllipticalEaseOut pNewValue,pDuration,pExponent
   try
      if pNewValue is not a number or pDuration is not a number then 
         return "error: Parameter is not a number"
         exit moveEllipticalEaseOut
      end if
      if pExponent is not a number and pExponent is not empty then
         return "error: Parameter is not a number"
         exit moveEllipticalEaseOut
      end if
      if pExponent is empty then put 2 into pExponent
      set the moveElliptical["easeStart"] of the target to the moveElliptical["step"] of the target
      set the moveElliptical["easeEnd"] of the target to pNewValue
      set the moveElliptical["easeDuration"] of the target to pDuration
      set the moveElliptical["easeExponent"] of the target to pExponent
      set the moveElliptical["easeStartTime"] of the target to the milliseconds
      send "moveEllipticalChangeStepOut" to the target
   catch theError
      return theError
   end try
end moveEllipticalEaseOut

on moveEllipticalChangeStepOut
   local tDuration,tElapsed,tEnd,tExponent
   local tMillisecs,tNewStep,tStart
   try
      put the moveElliptical["easeStart"] of the target into tStart
      put the moveElliptical["easeEnd"] of the target into tEnd
      put the moveElliptical["easeDuration"] of the target into tDuration
      put the moveElliptical["easeExponent"] of the target into tExponent
      put the moveElliptical["easeStartTime"] of the target into tMillisecs
      put the milliseconds-tMillisecs into tElapsed
      put aeEaseOut(tStart,tEnd,tDuration,tElapsed,tExponent) into tNewStep
      set the moveElliptical["step"] of the target to tNewStep
      if tNewStep<>tEnd then send "moveEllipticalChangeStepOut" to the target in 20 milliseconds
   catch theError
      return theError
   end try
end moveEllipticalChangeStepOut

on moveEllipticalEaseInOut pNewValue,pDuration,pExponent
   try
      if pNewValue is not a number or pDuration is not a number then 
         return "error: Parameter is not a number"
         exit moveEllipticalEaseInOut
      end if
      if pExponent is not a number and pExponent is not empty then
         return "error: Parameter is not a number"
         exit moveEllipticalEaseInOut
      end if
      if pExponent is empty then put 2 into pExponent
      set the moveElliptical["easeStart"] of the target to the moveElliptical["step"] of the target
      set the moveElliptical["easeEnd"] of the target to pNewValue
      set the moveElliptical["easeDuration"] of the target to pDuration
      set the moveElliptical["easeExponent"] of the target to pExponent
      set the moveElliptical["easeStartTime"] of the target to the milliseconds
      send "moveEllipticalChangeStepInOut" to the target
   catch theError
      return theError
   end try
end moveEllipticalEaseInOut

on moveEllipticalChangeStepInOut
   local tDuration,tElapsed,tEnd,tExponent
   local tMillisecs,tNewStep,tStart
   try
      put the moveElliptical["easeStart"] of the target into tStart
      put the moveElliptical["easeEnd"] of the target into tEnd
      put the moveElliptical["easeDuration"] of the target into tDuration
      put the moveElliptical["easeExponent"] of the target into tExponent
      put the moveElliptical["easeStartTime"] of the target into tMillisecs
      put the milliseconds-tMillisecs into tElapsed
      put aeEaseInOut(tStart,tEnd,tDuration,tElapsed,tExponent) into tNewStep
      set the moveElliptical["step"] of the target to tNewStep
      if tNewStep<>tEnd then send "moveEllipticalChangeStepInOut" to the target in 20 milliseconds
   catch theError
      return theError
   end try
end moveEllipticalChangeStepInOut

-- linear easing
on moveLinearEaseIn pNewValue,pDuration,pExponent
   try
      if pNewValue is not a number or pDuration is not a number then 
         return "error: Parameter is not a number"
         exit moveLinearEaseIn
      end if
      if pExponent is not a number and pExponent is not empty then
         return "error: Parameter is not a number"
         exit moveLinearEaseIn
      end if
      if pExponent is empty then put 2 into pExponent
      set the moveLinear["easeStart"] of the target to the moveLinear["step"] of the target
      set the moveLinear["easeEnd"] of the target to pNewValue
      set the moveLinear["easeDuration"] of the target to pDuration
      set the moveLinear["easeExponent"] of the target to pExponent
      set the moveLinear["easeStartTime"] of the target to the milliseconds
      send "moveLinearChangeStepIn" to the target
   catch theError
      return theError
   end try
   
end moveLinearEaseIn

on moveLinearChangeStepIn
   local tDuration,tElapsed,tEnd,tExponent
   local tMillisecs,tNewStep,tStart
   try
      put the moveLinear["easeStart"] of the target into tStart
      put the moveLinear["easeEnd"] of the target into tEnd
      put the moveLinear["easeDuration"] of the target into tDuration
      put the moveLinear["easeExponent"] of the target into tExponent
      put the moveLinear["easeStartTime"] of the target into tMillisecs
      put the milliseconds-tMillisecs into tElapsed
      put aeEaseIn(tStart,tEnd,tDuration,tElapsed,tExponent) into tNewStep
      set the moveLinear["step"] of the target to tNewStep
      if tNewStep<>tEnd then send "moveLinearChangeStepIn" to the target in 20 milliseconds
   catch theError
      return theError
   end try
   
end moveLinearChangeStepIn

on moveLinearEaseOut pNewValue,pDuration,pExponent
   try
      if pNewValue is not a number or pDuration is not a number then 
         return "error: Parameter is not a number"
         exit moveLinearEaseOut
      end if
      if pExponent is not a number and pExponent is not empty then
         return "error: Parameter is not a number"
         exit moveLinearEaseOut
      end if
      if pExponent is empty then put 2 into pExponent
      set the moveLinear["easeStart"] of the target to the moveLinear["step"] of the target
      set the moveLinear["easeEnd"] of the target to pNewValue
      set the moveLinear["easeDuration"] of the target to pDuration
      set the moveLinear["easeExponent"] of the target to pExponent
      set the moveLinear["easeStartTime"] of the target to the milliseconds
      send "moveLinearChangeStepOut" to the target
   catch theError
      return theError
   end try
   
end moveLinearEaseOut

on moveLinearChangeStepOut
   local tDuration,tElapsed,tEnd,tExponent
   local tMillisecs,tNewStep,tStart
   try
      put the moveLinear["easeStart"] of the target into tStart
      put the moveLinear["easeEnd"] of the target into tEnd
      put the moveLinear["easeDuration"] of the target into tDuration
      put the moveLinear["easeExponent"] of the target into tExponent
      put the moveLinear["easeStartTime"] of the target into tMillisecs
      put the milliseconds-tMillisecs into tElapsed
      put aeEaseOut(tStart,tEnd,tDuration,tElapsed,tExponent) into tNewStep
      set the moveLinear["step"] of the target to tNewStep
      if tNewStep<>tEnd then send "moveLinearChangeStepOut" to the target in 20 milliseconds
   catch theError
      return theError
   end try
   
end moveLinearChangeStepOut

on moveLinearEaseInOut pNewValue,pDuration,pExponent
   try
      if pNewValue is not a number or pDuration is not a number then 
         return "error: Parameter is not a number"
         exit moveLinearEaseInOut
      end if
      if pExponent is not a number and pExponent is not empty then
         return "error: Parameter is not a number"
         exit moveLinearEaseInOut
      end if
      if pExponent is empty then put 2 into pExponent
      set the moveLinear["easeStart"] of the target to the moveLinear["step"] of the target
      set the moveLinear["easeEnd"] of the target to pNewValue
      set the moveLinear["easeDuration"] of the target to pDuration
      set the moveLinear["easeExponent"] of the target to pExponent
      set the moveLinear["easeStartTime"] of the target to the milliseconds
      send "moveLinearChangeStepInOut" to the target
   catch theError
      return theError
   end try
   
end moveLinearEaseInOut

on moveLinearChangeStepInOut
   local tDuration,tElapsed,tEnd,tExponent
   local tMillisecs,tNewStep,tStart
   try
      put the moveLinear["easeStart"] of the target into tStart
      put the moveLinear["easeEnd"] of the target into tEnd
      put the moveLinear["easeDuration"] of the target into tDuration
      put the moveLinear["easeExponent"] of the target into tExponent
      put the moveLinear["easeStartTime"] of the target into tMillisecs
      put the milliseconds-tMillisecs into tElapsed
      put aeEaseInOut(tStart,tEnd,tDuration,tElapsed,tExponent) into tNewStep
      set the moveLinear["step"] of the target to tNewStep
      if tNewStep<>tEnd then send "moveLinearChangeStepInOut" to the target in 20 milliseconds
   catch theError
      return theError
   end try
end moveLinearChangeStepInOut

-- Polygonal easing
on movePolygonalEaseIn pNewValue,pDuration,pExponent
   try
      if pNewValue is not a number or pDuration is not a number then 
         return "error: Parameter is not a number"
         exit movePolygonalEaseIn
      end if
      if pExponent is not a number and pExponent is not empty then
         return "error: Parameter is not a number"
         exit movePolygonalEaseIn
      end if
      if pExponent is empty then put 2 into pExponent
      set the movePolygonal["easeStart"] of the target to the movePolygonal["step"] of the target
      set the movePolygonal["easeEnd"] of the target to pNewValue
      set the movePolygonal["easeDuration"] of the target to pDuration
      set the movePolygonal["easeExponent"] of the target to pExponent
      set the movePolygonal["easeStartTime"] of the target to the milliseconds
      send "movePolygonalChangeStepIn" to the target
   catch theError
      return theError
   end try
   
end movePolygonalEaseIn

on movePolygonalChangeStepIn
   local tDuration,tElapsed,tEnd,tExponent
   local tMillisecs,tNewStep,tStart
   try
      put the movePolygonal["easeStart"] of the target into tStart
      put the movePolygonal["easeEnd"] of the target into tEnd
      put the movePolygonal["easeDuration"] of the target into tDuration
      put the movePolygonal["easeExponent"] of the target into tExponent
      put the movePolygonal["easeStartTime"] of the target into tMillisecs
      put the milliseconds-tMillisecs into tElapsed
      put aeEaseIn(tStart,tEnd,tDuration,tElapsed,tExponent) into tNewStep
      set the movePolygonal["step"] of the target to tNewStep
      if tNewStep<>tEnd then send "movePolygonalChangeStepIn" to the target in 20 milliseconds
   catch theError
      return theError
   end try
   
end movePolygonalChangeStepIn

on movePolygonalEaseOut pNewValue,pDuration,pExponent
   try
      if pNewValue is not a number or pDuration is not a number then 
         return "error: Parameter is not a number"
         exit movePolygonalEaseOut
      end if
      if pExponent is not a number and pExponent is not empty then
         return "error: Parameter is not a number"
         exit movePolygonalEaseOut
      end if
      if pExponent is empty then put 2 into pExponent
      set the movePolygonal["easeStart"] of the target to the movePolygonal["step"] of the target
      set the movePolygonal["easeEnd"] of the target to pNewValue
      set the movePolygonal["easeDuration"] of the target to pDuration
      set the movePolygonal["easeExponent"] of the target to pExponent
      set the movePolygonal["easeStartTime"] of the target to the milliseconds
      send "movePolygonalChangeStepOut" to the target
   catch theError
      return theError
   end try
   
end movePolygonalEaseOut

on movePolygonalChangeStepOut
   local tDuration,tElapsed,tEnd,tExponent
   local tMillisecs,tNewStep,tStart
   try
      put the movePolygonal["easeStart"] of the target into tStart
      put the movePolygonal["easeEnd"] of the target into tEnd
      put the movePolygonal["easeDuration"] of the target into tDuration
      put the movePolygonal["easeExponent"] of the target into tExponent
      put the movePolygonal["easeStartTime"] of the target into tMillisecs
      put the milliseconds-tMillisecs into tElapsed
      put aeEaseOut(tStart,tEnd,tDuration,tElapsed,tExponent) into tNewStep
      set the movePolygonal["step"] of the target to tNewStep
      if tNewStep<>tEnd then send "movePolygonalChangeStepOut" to the target in 20 milliseconds
   catch theError
      return theError
   end try
   
end movePolygonalChangeStepOut

on movePolygonalEaseInOut pNewValue,pDuration,pExponent
   try
      if pNewValue is not a number or pDuration is not a number then 
         return "error: Parameter is not a number"
         exit movePolygonalEaseInOut
      end if
      if pExponent is not a number and pExponent is not empty then
         return "error: Parameter is not a number"
         exit movePolygonalEaseInOut
      end if
      if pExponent is empty then put 2 into pExponent
      set the movePolygonal["easeStart"] of the target to the movePolygonal["step"] of the target
      set the movePolygonal["easeEnd"] of the target to pNewValue
      set the movePolygonal["easeDuration"] of the target to pDuration
      set the movePolygonal["easeExponent"] of the target to pExponent
      set the movePolygonal["easeStartTime"] of the target to the milliseconds
      send "movePolygonalChangeStepInOut" to the target
   catch theError
      return theError
   end try
   
end movePolygonalEaseInOut

on movePolygonalChangeStepInOut
   local bummba
   local tDuration,tElapsed,tEnd,tExponent
   local tMillisecs,tNewStep,tStart
   try
      put the movePolygonal["easeStart"] of the target into tStart
      put the movePolygonal["easeEnd"] of the target into tEnd
      put the movePolygonal["easeDuration"] of the target into tDuration
      put the movePolygonal["easeExponent"] of the target into tExponent
      put the movePolygonal["easeStartTime"] of the target into tMillisecs
      put the milliseconds-tMillisecs into tElapsed
      put aeEaseInOut(tStart,tEnd,tDuration,tElapsed,tExponent) into tNewStep
      set the movePolygonal["step"] of the target to tNewStep
      if tNewStep<>tEnd then send "movePolygonalChangeStepInOut" to the target in 20 milliseconds
   catch theError
      return theError
   end try
   
end movePolygonalChangeStepInOut


-- end added in 2.9

-- easing functions

/**

Summary: Returns an ease-in interpolated value

Parameters:
pStart: The starting value
pEnd: The ending value
pDuration: The total duration of the interpolation
pElapsedTime: The elapsed time since the start of the interpolation
pExponent: The exponent to control the easing curve

Returns: The computed result
*/
function aeEaseIn pStart,pEnd,pDuration,pElapsedTime,pExponent
   try
      if pDuration<=pElapsedTime then return pEnd
      if pElapsedTime<=0 then return pStart
      return pStart+((pElapsedTime/pDuration)^pExponent)*(pEnd-pStart)
   catch theError
      return theError
   end try
   
end aeEaseIn

/**

Summary: Returns an ease-out interpolated value

Parameters:
pStart: The starting value
pEnd: The ending value
pDuration: The total duration of the interpolation
pElapsedTime: The elapsed time since the start of the interpolation
pExponent: The exponent to control the easing curve

Returns: The computed result
*/
function aeEaseOut pStart,pEnd,pDuration,pElapsedTime,pExponent
   try
      if pDuration<=pElapsedTime then return pEnd
      if pElapsedTime<=0 then return pStart
      return pStart+(1-(1-(pElapsedTime/pDuration))^pExponent)*(pEnd-pStart)
   catch theError
      return theError
   end try
   
end aeEaseOut

/**

Summary: Returns an ease-in-out interpolated value

Parameters:
pStart: The starting value
pEnd: The ending value
pDuration: The total duration of the interpolation
pElapsedTime: The elapsed time since the start of the interpolation
pExponent: The exponent to control the easing curve

Returns: The computed result
*/
function aeEaseInOut pStart,pEnd,pDuration,pElapsedTime,pExponent
   try
      if pDuration<=pElapsedTime then return pEnd
      if pElapsedTime<=0 then return pStart
      if pElapsedTime/pDuration<0.5 then
         return pStart+((((pElapsedTime/pDuration)*2)^pExponent)/2)*(pEnd-pStart)
      else
         return pStart+(0.5+aeMathEaseOut((pElapsedTime/pDuration-0.5)*2,pExponent)/2)*(pEnd-pStart)
      end if
   catch theError
      return theError
   end try
   
end aeEaseInOut

/**

Summary: Returns a mathematically computed ease-out value

Parameters:
pR: The ratio of elapsed time to duration
pExponent: The exponent to control the easing curve

Returns: The computed result
*/
function aeMathEaseOut pR,pExponent
   try
      return 1-(1-pR)^pExponent
   catch theError
      return theError
   end try
end aeMathEaseOut

-- isometric drawing and 3d

/**

Summary: Converts an isometric X coordinate to screen X

Returns: The computed result
*/
function isoToScreenX
   local i,pOriginX,pX,pY
   local pZ,theValue,tXCartesian
   try
      --assemble one string from parameters
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      --check syntax
      if the number of items of theValue<>4 then 
         return "Error: Syntax is isoToScreenX(pX,pY,pZ,pOriginX)"
         exit isoToScreenX
      end if
      --check syntax
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit isoToScreenX
            exit repeat
         end if
      end repeat
      put item 1 of theValue into pX
      put item 2 of theValue into pY
      put item 3 of theValue into pZ
      put item 4 of theValue into pOriginX
      -- generates x in cartesian coordinates from classic iso view
      --put (pX-pZ)*cos(0.46365) into tXCartesian
      --commented line might be needed for changeing cam position later on
      put (pX-pZ)*0.894426 into tXCartesian
      return tXCartesian+pOriginX
   catch theError
      return theError
   end try
end isoToScreenX

/**

Summary: Converts an isometric Y coordinate to screen Y

Returns: The computed result
*/
function isoToScreenY
   local i,pOriginY,pX,pY
   local pZ,theValue,tYCartesian
   try
      --assemble one string from parameters
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      --check syntax
      if the number of items of theValue<>4 then 
         return "Error: Syntax is isoToScreenY(pX,pY,pZ,pOriginY)"
         exit isoToScreenY
      end if
      --check syntax
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit isoToScreenY
            exit repeat
         end if
      end repeat
      put item 1 of theValue into pX
      put item 2 of theValue into pY
      put item 3 of theValue into pZ
      put item 4 of theValue into pOriginY
      
      --put pY+(pX+pZ)*sin(0.46365) into tYCartesian
      put pY+(pX+pZ)*0.447216 into tYCartesian
      return -tYCartesian  +pOriginY
   catch theError
      return theError
   end try
end isoToScreenY

/**

Summary: Converts isometric coordinates to screen coordinates

Returns: The computed result
*/
function isoToScreen
   local i,pOriginX,pOriginY,pX
   local pY,pZ,theValue
   try
      --assemble one string from parameters
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      --check syntax
      if the number of items of theValue<>5 then 
         return "Error: Syntax is isoToScreen(pX,pY,pZ,pOriginX,pOriginY)"
         exit isoToScreen
      end if
      --check syntax
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit isoToScreen
            exit repeat
         end if
      end repeat
      put item 1 of theValue into pX
      put item 2 of theValue into pY
      put item 3 of theValue into pZ
      put item 4 of theValue into pOriginX
      put item 5 of theValue into pOriginY
      return round(isoToScreenX(pX,pY,pZ,pOriginX)),round(isoToScreenY(pX,pY,pZ,pOriginY))
   catch theError
      return theError
   end try
end isoToScreen

on drawIsoLine
   local i,pOriginX,pOriginY,pX1
   local pX2,pY1,pY2,pZ1
   local pZ2,theValue,tPointList
   try
      --assemble one string from parameters
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      --check syntax
      if the number of items of theValue<>8 then 
         return "Error: Syntax is drawIsoLine pX1,pY1,pZ1,pX2,pY2,pZ2,pOriginX,pOriginY"
         exit drawIsoLine
      end if
      --check syntax
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit drawIsoLine
            exit repeat
         end if
      end repeat
      put item 1 of theValue into pX1
      put item 2 of theValue into pY1
      put item 3 of theValue into pZ1
      put item 4 of theValue into pX2
      put item 5 of theValue into pY2
      put item 6 of theValue into pZ2
      put item 7 of theValue into pOriginX
      put item 8 of theValue into pOriginY
      put isoToScreen(pX1,pY1,pZ1,pOriginX,pOriginY) into tPointList
      put cr after tPointList
      put isoToScreen(pX2,pY2,pZ2,pOriginX,pOriginY) after tPointList
      create grc "isoLine"
      set the style of it to "curve"
      set the points of it to tPointList
   catch theError
      return theError
   end try
end drawIsoLine

on drawIsoBox
   local i,pA,pB,pC
   local pointlist,pOriginX,pOriginY,pX
   local pY,pZ,theValue
   try
      --assemble one string from parameters
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      --check syntax
      if the number of items of theValue<>8 then 
         return "Error: Syntax is drawIsoBox(pX,pY,pZ,pA,pB,pC,pOriginX,pOriginY)"
         exit drawIsoBox
      end if
      --check syntax
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit drawIsoBox
            exit repeat
         end if
      end repeat
      put item 1 of theValue into pX
      put item 2 of theValue into pY
      put item 3 of theValue into pZ
      put item 4 of theValue into pA
      put item 5 of theValue into pB
      put item 6 of theValue into pC
      put item 7 of theValue into pOriginX
      put item 8 of theValue into pOriginY
      put isoToScreen(pX,pY,pZ,pOriginX,pOriginY)&cr into pointlist
      put isoToScreen(pX+pA,pY,pZ,pOriginX,pOriginY) &cr after pointlist
      put isoToScreen(px+pa,py+pb,pz,pOriginX,pOriginY)&cr after pointlist
      put isoToScreen(px, py+pb, pz,pOriginX,pOriginY)&cr after pointlist
      put isoToScreen(px, py, pz,pOriginX,pOriginY)&cr after pointlist
      put cr&isoToScreen(px, py+pb, pz,pOriginX,pOriginY)&cr after pointlist
      put isoToScreen(px+pa, py+pb, pz,pOriginX,pOriginY)&cr after pointlist
      put isoToScreen(px+pa, py+pb, pz+pc,pOriginX,pOriginY)&cr after pointlist
      put isoToScreen(px, py+pb, pz+pc,pOriginX,pOriginY)&cr after pointlist
      put isoToScreen(px, py+pb, pz,pOriginX,pOriginY)&cr after pointlist
      put cr&isoToScreen(px, py, pz,pOriginX,pOriginY)&cr after pointlist
      put isoToScreen(px, py+pb, pz,pOriginX,pOriginY)&cr after pointlist
      put isoToScreen(px, py+pb, pz+pc,pOriginX,pOriginY)&cr after pointlist
      put isoToScreen(px, py, pz+pc,pOriginX,pOriginY)&cr after pointlist
      put isoToScreen(px,py, pz,pOriginX,pOriginY)&cr after pointlist
      create grc "isoBox"
      set the style of it to "curve"
      set the points of it to pointList
   catch theError
      return theError
   end try
end drawIsoBox

/**

Summary: Rotates a point in isometric space

Returns: The computed result
*/
function rotateIsoPoint
   local cX,cY,cZ,i
   local newX,newY,newZ,pFocalLength
   local pX,pXRotation,pY,pYRotation
   local pZ,pZRotation,sX,sY
   local sZ,theValue,xY,xZ
   local yX,yZ,zX,zY
   try
      --assemble one string from parameters
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      --check syntax
      if the number of items of theValue<6 then 
         return "Error: Syntax is rotateIsoPoint(pX,pY,pZ,pXRotation,pYrotation,pZrotation)"
         exit rotateIsoPoint
      end if
      --check syntax
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit rotateIsoPoint
            exit repeat
         end if
      end repeat
      put item 1 of theValue into pX
      put item 2 of theValue into pY
      put item 3 of theValue into pZ
      put item 4 of theValue into pXRotation
      put item 5 of theValue into pYRotation
      put item 6 of theValue into pZRotation
      put item 7 of theValue into pFocalLength
      put pXrotation*pi/180 into pXrotation
      put pYrotation*pi/180 into pYrotation
      put pZrotation*pi/180 into pZrotation
      put sin(pXRotation) into sX
      put cos(pXRotation) into cX
      put sin(pYRotation) into sY
      put cos(pYRotation) into cY
      put sin(pZRotation) into sZ
      put cos(pZRotation) into cZ
      -- rotation around x
      put cx*pY - sx*pZ into xY
      put sx*pY + cx*pZ into xZ
      -- rotation around y
      put cy*xz - sy*pX into yZ
      put sy*xz + cy*pX into yX
      -- rotation around z
      put cz*yx - sz*xy into zX
      put sz*yx + cz*xy into zY
      
      put zX into newX
      put zY into newY
      put yZ into newZ
      
      return newX,newY,newZ
   catch theError
      return theError
   end try
end rotateIsoPoint

/**

Summary: Projects a 3D point onto the 2D screen

Returns: The computed result
*/
function ae3dConvertToScreen
   local i,pFocalLength,pOriginX,pOriginy
   local pX,pY,pZ,theValue
   local tScalefactor,tXCartesian,tYCartesian
   try
      --assemble one string from parameters
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      --check syntax
      if the number of items of theValue<5 then 
         return "Error: Syntax is ae3dConvertToScreen(pX,pY,pZ,pOriginX,pOriginY[,pFocalLength])"
         exit ae3dConvertToScreen
      end if
      --check syntax
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit ae3dConvertToScreen
            exit repeat
         end if
      end repeat
      put item 1 of theValue into pX
      put item 2 of theValue into pY
      put item 3 of theValue into pZ
      put item 4 of theValue into pOriginX
      put item 5 of theValue into pOriginy
      put item 6 of theValue into pFocalLength
      if pFocalLength is empty then put 300 into pFocalLength
      put ae3dScaleRatio(pZ,pFocalLength) into tScalefactor
      put px*tScalefactor into tXCartesian
      put pY*tScalefactor into tYCartesian
      return round(tXCartesian+pOriginX),round(-tYCartesian+pOriginY)
   catch theError
      return theError
   end try
end ae3dConvertToScreen



/**

Summary: Rotates a point in 3D space around the X, Y and Z axes

Returns: The computed result
*/
function rotate3DPoint
   local cX,cY,cZ,i
   local newX,newY,newZ,pFocalLength
   local pX,pXRotation,pY,pYRotation
   local pZ,pZRotation,tScalefactor,sX
   local sY,sZ,theValue,xY
   local xZ,yX,yZ,zX
   local zY
   try
      --assemble one string from parameters
      repeat with i=1 to paramcount()
         if i<paramcount() then
            put param(i)&"," after theValue
         else
            put param(i) after theValue
         end if
      end repeat
      --check syntax
      if the number of items of theValue<6 then 
         return "Error: Syntax is rotate3DPoint(pX,pY,pZ,pXRotation,pYrotation,pZrotation[,pFocalLength])"
         exit rotate3DPoint
      end if
      --check syntax
      repeat with i=1 to the number of items of theValue
         if item i of theValue is not a number then
            return "Error: All Parameters must be numbers!"
            exit rotate3DPoint
            exit repeat
         end if
      end repeat
      put item 1 of theValue into pX
      put item 2 of theValue into pY
      put item 3 of theValue into pZ
      put item 4 of theValue into pXRotation
      put item 5 of theValue into pYRotation
      put item 6 of theValue into pZRotation
      put item 7 of theValue into pFocalLength
      if pFocalLength is empty then put 300 into pFocalLength
      put pXrotation*pi/180 into pXrotation
      put pYrotation*pi/180 into pYrotation
      put pZrotation*pi/180 into pZrotation
      put sin(pXRotation) into sX
      put cos(pXRotation) into cX
      put sin(pYRotation) into sY
      put cos(pYRotation) into cY
      put sin(pZRotation) into sZ
      put cos(pZRotation) into cZ
      -- rotation around x
      put cx*pY - sx*pZ into xY
      put sx*pY + cx*pZ into xZ
      -- rotation around y
      put cy*xz - sy*pX into yZ
      put sy*xz + cy*pX into yX
      -- rotation around z
      put cz*yx - sz*xy into zX
      put sz*yx + cz*xy into zY
      put pfocalLength/(pfocalLength + yz) into tScalefactor
      put zX * tScalefactor into newX
      put zY * tScalefactor into newY
      put yZ into newZ
      
      return newX,newY,newZ
   catch theError
      return theError
   end try
end rotate3DPoint

/**

Summary: Returns the scale ratio for a point at a given Z depth

Parameters:
pZ: The Z coordinate of the point
pFocalLength: The focal length of the camera

Returns: The computed result
*/
function ae3dScaleRatio pZ,pFocalLength
   try
      return pFocalLength/(pFocalLength+pZ)
   catch theError
      return theError
   end try
end ae3dScaleRatio

-- unSupported

/**

Summary: Draws a 3D box primitive

Parameters:
px: The X coordinate of the box
py: The Y coordinate of the box
pz: The Z coordinate of the box
pa: The width of the box along the X axis
pb: The height of the box along the Y axis
pc: The depth of the box along the Z axis

Returns: The computed result
*/
function AePrimitive_Box px,py,pz,pa,pb,pc
   local pointlist
   try
      put pX,pY,pZ&cr into pointlist
      put pX+pA,pY,pZ &cr after pointlist
      put px+pa,py+pb,pz&cr after pointlist
      put px, py+pb, pz&cr after pointlist
      put px, py, pz&cr after pointlist
      put cr&px, py+pb, pz&cr after pointlist
      put px+pa, py+pb, pz&cr after pointlist
      put px+pa, py+pb, pz+pc&cr after pointlist
      put px, py+pb, pz+pc&cr after pointlist
      put px, py+pb, pz&cr after pointlist
      put cr&px, py, pz&cr after pointlist
      put px, py+pb, pz&cr after pointlist
      put px, py+pb, pz+pc&cr after pointlist
      put px, py, pz+pc&cr after pointlist
      put px,py, pz&cr after pointlist
      return pointList
   catch theError
      return theError
   end try
end AePrimitive_Box

on draw3dBox pX,pY,pZ,pA,pB,pC,pOriginX,pOriginY,pXRot,pYrot,pZrot,pFocalLength
   local pointlist,pointlist2,theLine
   try
      put rotate3DPoint(pX,pY,pZ,pxRot,pYRot,pZRot,pFocalLength)&cr into pointlist
      put rotate3DPoint(pX+pA,pY,pZ,pxRot,pYRot,pZRot,pFocalLength) &cr after pointlist
      put rotate3DPoint(px+pa,py+pb,pz,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      put rotate3DPoint(px, py+pb, pz,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      put rotate3DPoint(px, py, pz,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      put cr&rotate3dpoint(px, py+pb, pz,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      put rotate3DPoint(px+pa, py+pb, pz,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      put rotate3DPoint(px+pa, py+pb, pz+pc,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      put rotate3DPoint(px, py+pb, pz+pc,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      put rotate3DPoint(px, py+pb, pz,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      put cr&rotate3dpoint(px, py, pz,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      put rotate3DPoint(px, py+pb, pz,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      put rotate3DPoint(px, py+pb, pz+pc,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      put rotate3DPoint(px, py, pz+pc,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      put rotate3DPoint(px,py, pz,pxRot,pYRot,pZRot,pFocalLength)&cr after pointlist
      
      repeat for each line theLine in pointList
         if theLine is not empty then
            
            put ae3dConvertToScreen(item 1 of theLine,item 2 of theLine,item 3 of theLine,pOriginX,pOriginY,pFocalLength)&cr after pointlist2
         else
            put cr&cr after pointList2
         end if
      end repeat
      
      create grc "3dBox"
      set the style of it to "curve"
      set the points of it to pointList2
   catch theError
      return theError
   end try
   
end draw3dBox

on draw3dLine pX1,pY1,pZ1,pX2,pY2,pZ2,pOriginX,pOriginY,pViewAngle,pFocalLength
   try
      create grc "3dLine"
      set the style of it to "curve"
      set the points of it to ae3dConvertToScreen(pX1,pY1,pZ1,pOriginX,pOriginY,pviewAngle,pFocalLength)&cr&ae3dConvertToScreen(pX2,pY2,pZ2,pOriginX,pOriginY,pViewAngle,pFocalLength)
   catch theError
      return theError
   end try
end draw3dLine

-- 2.1 update

on aeChangeTextSize pChange
   local counter,newHTML,testItem,theHtml
   local theItem,theSize
   if pChange is not a number then
      return "Error: parameter must be a number!"
      exit aeChangeTextSize
   end if
   if there is no field the target then
      return "Error: Target must be a field!"&&the target
      exit aeChangeTextSize
   end if
   put the htmltext of the target into theHtml
   if the effective textsize of the target+pChange<8 then
      return "Smallest size reached"
      exit aeChangeTextSize
   end if
   set the textsize of the target to \
         the effective textsize of the target+pChange
   replace "size="&quote with "size="&numToChar(1500) in theHTML
   set the itemdel to numtochar(1500)
   if the number of items of theHTML<>1 then
      put 0 into testItem
      repeat for each item theItem in theHTML
         add 1 to testItem
         if testItem<>1 then
            put offset(quote,theitem) into counter
            put char 1 to counter-1 of theItem into theSize
            put theSize+pChange into theSize
            if theSize<8 then
               set the textsize of the target to \
                     the effective textsize of the target-pChange
               return "Smallest size reached!"
               exit aeChangeTextSize
            end if
            put theSize into char 1 to counter-1 of theItem
            put quote&theItem after newHTML
         else
            put theItem into newHTML
            next repeat
         end if
      end repeat
      set the htmltext of the target to newHTML
   end if
   --put the htmlText of the target
end aeChangeTextSize

--> 2.9 _toplevel handlers

--> 2.9 moving stuff
local tAeEasing,tAEEasingCircular,sAEFrameRate,sAERealFrameRate,sAELockMoves

/**

Summary: Returns the current average animation frame rate

Returns: The computed result
*/
function aeGetAverageFrameRate
   if sAERealFrameRate is not empty then
      return sAERealFrameRate
   else
      if sAEFrameRate is not empty then
         return sAEFrameRate
      else return 0
   end if
end aeGetAverageFrameRate

on aeSetFrameRate pRate
   if pRate is not a number then 
      aeResetFrameRate
      return "error: frame rate is not a number"
   else
      put pRate into sAEFrameRate
   end if
end aeSetFrameRate

private command aeResetFrameRate
   put 25 into sAEFrameRate
   put 25 into sAERealFrameRate
end aeResetFrameRate



on aeChangeAngleCircular
   local tControl,tX,tY,tRadius,tNewAngle,tDuration,tMethod,theValue,tCurrentAngle,tDirection
   if sAEFrameRate is empty then aeResetFrameRate
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tControl
   put item 2 of theValue into tX
   put item 3 of theValue into tY
   put item 4 of theValue into tRadius
   put item 5 of theValue into tNewAngle
   put item 6 of theValue into tDuration
   put item 7 of theValue into tDirection
   put item 8 of theValue into tMethod
   if there is no tControl then return "error:"&&tControl&&"is not a valid control or group or stack"
   if word 1 of the long name of tControl is "card" then return "error: can not move a card"
   if tX is not an integer then return "error:"&&tX&","&tY&&"is not a valid point"
   if tY is not an integer then return "error:"&&tX&","&tY&&"is not a valid point"
   if tRadius is not a number then return "error:" && tRadius && "is not a number (radius)"
   if tNewAngle is not a number then return "error:" && tNewAngle && "is not a number (newAngle)"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tDuration&&"is not a valid easing method. Must be in,out or inOut"
   put findAngleX(tX,tY,item 1 of the the loc of tControl,item 2 of the loc of tControl) into tCurrentAngle
   switch tDirection
      case "clockwise"
         if tNewAngle < tCurrentAngle then add 360 to tNewAngle
         break
      case "counterClockwise"
         if tNewAngle > tCurrentAngle then add 360 to tCurrentAngle
         break
   end switch
   if not sAELockMoves then
      put tX,tY,tDuration,tMethod,tCurrentAngle,tNewAngle,tRadius,the milliseconds into tAeEasingCircular[the long id of tControl]
      if "aeGeneral" is not in the pendingmessages then
         aeGeneral
      end if
   else
      put tX,tY,tDuration,tMethod,tCurrentAngle,tNewAngle,tRadius,"pending" into tAeEasingCircular[the long id of tControl]
   end if
end aeChangeAngleCircular

on aeMoveTo
   local tControl,tX,tY,tDuration,tMethod,theValue
   if sAEFrameRate is empty then aeResetFrameRate
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tControl
   put item 2 of theValue into tX
   put item 3 of theValue into tY
   put item 4 of theValue into tDuration
   put item 5 of theValue into tMethod
   if there is no tControl then return "error:"&&tControl&&"is not a valid control or group or stack"
   if word 1 of the long name of tControl is "card" then return "error: can not move a card"
   if tX is not an integer then return "error:"&&tX&","&tY&&"is not a valid point"
   if tY is not an integer then return "error:"&&tX&","&tY&&"is not a valid point"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tDuration&&"is not a valid easing method. Must be in,out or inOut"
   if not sAELockMoves then
      put tX,tY,tDuration,tMethod,item 1 of the the loc of tControl,item 2 of the loc of tControl,the milliseconds into tAeEasing[the long id of tControl]
      if "aeGeneral" is not in the pendingmessages then
         aeGeneral
      end if
   else
      put tX,tY,tDuration,tMethod,item 1 of the the loc of tControl,item 2 of the loc of tControl,"pending" into tAeEasing[the long id of tControl]
   end if
end aeMoveTo

on aeLockMoves
   put true into sAELockMoves
end aeLockMoves

on aeUnlockMoves
   local tNow
   put the milliseconds into tNow
   repeat for each line pControl in the keys of tAEEasing
      if item 7 of tAEEasing[pControl] <> "pending" then next repeat
      put tNow into item 7 of tAEEasing[pControl]
   end repeat
   put false into sAeLockMoves
   if "aeGeneral" is not in the pendingmessages then aeGeneral
end aeUnlockMoves

local sTimeTaken

/**

Summary: Performs a general animation step on the target object
*/
command aeGeneral
   lock screen
   local tTimeLost,tFramesLost,tElapsed,tControls
   local tX,tY,tDuration,tMethod,tEndX,tStartX,tDestX,tStartY,tDestY,tStartTime
   if sAEFrameRate is not a number then aeResetFrameRate
   put the milliseconds into tElapsed
   put the milliseconds - sTimeTaken into tTimeLost
   put tTimeLost - (1000/ sAEFrameRate) into tTimeLost
   put ((1000/sAeFramerate) + tTimeLost) into tFramesLost
   if tFramesLost<>0 then
      put 1000/tFramesLost into tFramesLost
   end if
   if tFramesLost>sAEFrameRate then put sAEFrameRate into tFramesLost
   put tFramesLost into sAERealFrameRate
   put the millisecs into sTimeTaken
   --lock screen
   put the keys of tAEEasing into tControls
   repeat for each line pControl in tControls
      -- housekeeping!
      -- need to check if control really exists!
      if there is a pControl then
         send "aeEnterFrame"  && "aeMoveTo" to pcontrol
         put item 1 of tAEEasing[pControl] into tDestX 
         put item 2 of tAEEasing[pControl] into tDestY
         put item 3 of tAEEasing[pControl] into tDuration
         put item 4 of tAEEasing[pControl] into tMethod
         put item 5 of tAEEasing[pControl] into tStartX
         put item 6 of tAEEasing[pControl] into tStartY
         put item 7 of tAEEasing[pControl] into tStartTime
         if tStartTime="pending" then next repeat
         switch tMethod
            case "in"
               put aeEaseIn(tStartX,tDestX,tDuration,the milliseconds-tStartTime,3) into tX
               put aeEaseIn(tStartY,tDestY,tDuration,the milliseconds-tStartTime,3) into ty
               break
            case "out"
               put aeEaseOut(tStartX,tDestX,tDuration,the milliseconds-tStartTime,3) into tX
               put aeEaseOut(tStartY,tDestY,tDuration,the milliseconds-tStartTime,3) into ty
               break
            case "inOut"
               put aeEaseInOut(tStartX,tDestX,tDuration,the milliseconds-tStartTime,3) into tX
               put aeEaseInOut(tStartY,tDestY,tDuration,the milliseconds-tStartTime,3) into ty
               break
            case "bounce"
               put aeBounceEaseOut(tStartX,tDestX,tDuration,the milliseconds-tStartTime) into tX
               put aeBounceEaseOut(tStartY,tDestY,tDuration,the milliseconds-tStartTime) into ty
               break
            case "overshoot"
               put aeOverShootEaseOut(tStartX,tDestX,tDuration,the milliseconds-tStartTime) into tX
               put aeOverShootEaseOut(tStartY,tDestY,tDuration,the milliseconds-tStartTime) into ty
               break
            default
               put aeEaseIn(tStartX,tDestX,tDuration,the milliseconds-tStartTime,1) into tX
               put aeEaseIn(tStartY,tDestY,tDuration,the milliseconds-tStartTime,1) into ty
               break
         end switch
         if the milliseconds-tStartTime<tDuration then
            set the loc of pControl to tX,tY
         else
            if tDestx,tDestY is a point then
               set the loc of pControl to tDestX,tDestY
            end if
            delete variable tAeEasing[pControl]
            send "aeMoveDone" to pControl
         end if
         send "aeExitFrame" && "aeMoveTo" to pControl
      else
         delete variable tAeEasing[pControl]
      end if
   end repeat
   
   local tCenterX,tCenterY,tRadius,tStartAngle,tEndAngle,tNewAngle
   -- tX,tY,tDuration,tMethod,findAngleX(tX,tY,item 1 of the the loc of tControl,item 2 of the loc of tControl),"pending"
   put the keys of tAEEasingCircular into tControls
   repeat for each line pControl in tControls
      if there is a pControl then
         send "aeEnterFrame"  && "aeChangeAngleCircular" to pcontrol
         put item 1 of tAEEasingCircular[pControl] into tCenterX 
         put item 2 of tAEEasingCircular[pControl] into tCenterY
         put item 3 of tAEEasingCircular[pControl] into tDuration
         put item 4 of tAEEasingCircular[pControl] into tMethod
         put item 5 of tAEEasingCircular[pControl] into tStartAngle
         put item 6 of tAEEasingCircular[pControl] into tEndAngle
         put item 7 of tAEEasingCircular[pControl] into tRadius
         put item 8 of tAEEasingCircular[pControl] into tStartTime
         if tStartTime="pending" then next repeat
         --      put tRadius
         --      exit to top
         switch tMethod
            case "in"
               put aeEaseIn(tStartAngle,tEndAngle,tDuration,the milliseconds-tStartTime,3) into tNewAngle
               break
            case "out"
               put aeEaseOut(tStartAngle,tEndAngle,tDuration,the milliseconds-tStartTime,3) into tNewAngle
               break
            case "inOut"
               put aeEaseInOut(tStartAngle,tEndAngle,tDuration,the milliseconds-tStartTime,3) into tNewAngle
               break
            case "bounce"
               put aeBounceEaseOut(tStartAngle,tEndAngle,tDuration,the milliseconds-tStartTime) into tNewAngle
               break
            case "overshoot"
               put aeOverShootEaseOut(tStartAngle,tEndAngle,tDuration,the milliseconds-tStartTime) into tNewAngle
               break
            default
               put aeEaseIn(tStartAngle,tEndAngle,tDuration,the milliseconds-tStartTime,1) into tNewAngle
               break
         end switch
         if the milliseconds-tStartTime<tDuration then
            set the loc of pControl to pointOnCircle(tCenterX,tCenterY,tNewAngle,tRadius)
         else
            if tDestx,tDestY is a point then
               set the loc of pControl to pointOnCircle(tCenterX,tCenterY,tEndAngle,tRadius)
            end if
            delete variable tAeEasingCircular[pControl]
            send "aeMoveDone" to pControl
         end if
         send "aeExitFrame" && "aeChangeAngleCircular" to pControl
      else
         delete variable tAeEasingCircular[pControl]
      end if
   end repeat
   
   -- morphing
   put the keys of sAEMorphing into tControls
   repeat for each line pControl in tControls
      if there is a pControl then
         send "aeEnterFrame" && "aeMorphGraphic" to pControl
         if sAEMorphing[pControl]["StartTime"] <> "pending" then
            if the millisecs - sAEMorphing[pControl]["StartTime"] < sAEMorphing[pControl]["duration"] then
               --aeMorphPointlists pList1,pList2,pDuration,pCurrentTime,pEffect
               --put the millisecs & cr & aeMorphPointlists(sAEMorphing[pControl]["p1"],sAEMorphing[pControl]["p2"],sAEMorphing[pControl]["duration"],the milliSecs -sAEMorphing[pControl]["StartTime"] , sAEMorphing[pControl]["Effect"]  )
               set the points of pControl to aeMorphPointlists(sAEMorphing[pControl]["p1"],sAEMorphing[pControl]["p2"],sAEMorphing[pControl]["duration"],the milliSecs -sAEMorphing[pControl]["StartTime"] , sAEMorphing[pControl]["Effect"]  )
            else
               set the points of pControl to sAEMorphing[pControl]["p2"]
               delete variable sAEMorphing[pControl]
               send "aeMorphDone" to pControl
            end if
         end if
         send "aeExitFrame" && "aeMorphGraphic" to pControl
      else
         delete variable sAEMorphing[pControl]
      end if
   end repeat
   
   --  sAEMorphGradientRamp
   put the keys of sAEMorphGradientRamp into tControls
   
   repeat for each line pControl in tControls
      if there is a pControl then
         send "aeEnterFrame" && "aeMorphGradient" to pControl
         if sAEMorphGradientRamp[pControl]["StartTime"] <> "pending" then
            if the millisecs - sAEMorphGradientRamp[pControl]["StartTime"] < sAEMorphGradientRamp[pControl]["duration"] then
               --aeMorphPointlists pList1,pList2,pDuration,pCurrentTime,pEffect
               set the fillGradient["ramp"] of pControl to aeMorphGradient(sAEMorphGradientRamp[pControl]["p1"],sAEMorphGradientRamp[pControl]["p2"],sAEMorphGradientRamp[pControl]["duration"],the milliSecs -sAEMorphGradientRamp[pControl]["StartTime"] , sAEMorphGradientRamp[pControl]["Effect"] )
            else
               set the fillGradient["ramp"] of pControl to sAEMorphGradientRamp[pControl]["p2"]
               delete variable sAEMorphGradientRamp[pControl]
               send "aeMorphGradientDone" to pControl
            end if
         end if
         send "aeExitFrame" && "aeMorphGradient" to pControl
      else
         delete variable sAEMorphGradientRamp[pControl]
      end if
   end repeat
   
   -- rotate group
   
   put the keys of sAERotateGroup into tControls
   
   repeat for each line theGroup in tControls
      if there is a theGroup then
         send "aeEnterFrame" && "aeRotateGroup" to theGroup
         if sAERotateGroup[theGroup]["StartTime"] <> "pending" then
            put sAERotateGroup[theGroup]["duration"] into tDuration
            put sAERotateGroup[theGroup]["StartTime"] into tStartTime
            put sAERotateGroup[theGroup]["Effect"] into tMethod
            
            if the millisecs - sAERotateGroup[theGroup]["StartTime"] < tDuration then
               repeat for each line theControl in the keys of sAERotateGroup[theGroup]["controls"]
                  
                  put sAERotateGroup[theGroup]["Angle"] + sAERotateGroup[theGroup]["controls"][theControl]["Angle"] into tEndAngle
                  put sAERotateGroup[theGroup]["controls"][theControl]["Angle"] into tStartAngle
                  
                  switch tMethod
                     case "in"
                        put aeEaseIn(tStartAngle,tEndAngle,tDuration,the milliseconds-tStartTime,3) into tNewAngle
                        break
                     case "out"
                        put aeEaseOut(tStartAngle,tEndAngle,tDuration,the milliseconds-tStartTime,3) into tNewAngle
                        break
                     case "inOut"
                        put aeEaseInOut(tStartAngle,tEndAngle,tDuration,the milliseconds-tStartTime,3) into tNewAngle
                        break
                     case "bounce"
                        put aeBounceEaseOut(tStartAngle,tEndAngle,tDuration,the milliseconds-tStartTime) into tNewAngle
                        break
                     case "overshoot"
                        put aeOverShootEaseOut(tStartAngle,tEndAngle,tDuration,the milliseconds-tStartTime) into tNewAngle
                        break
                     default
                        put aeEaseIn(tStartAngle,tEndAngle,tDuration,the milliseconds-tStartTime,1) into tNewAngle
                        break
                  end switch
                  
                  /*
                  put item 1 of theValue into centerX
                  put item 2 of theValue into centerY
                  put item 3 of theValue into isAngle
                  put item 4 of theValue into isRadius
                  */
                  
                  set the loc of theControl to pointOnCircle(sAERotateGroup[theGroup]["center"],tNewAngle,sAERotateGroup[theGroup]["controls"][theControl]["Distance"])
               end repeat
               
            else
               repeat for each line theControl in the keys of sAERotateGroup[theGroup]["controls"]
                  put sAERotateGroup[theGroup]["Angle"] + sAERotateGroup[theGroup]["controls"][theControl]["Angle"] into tEndAngle
                  set the loc of theControl to pointOnCircle(sAERotateGroup[theGroup]["center"],tEndAngle,sAERotateGroup[theGroup]["controls"][theControl]["Distance"])
               end repeat
               delete variable sAERotateGroup[theGroup]
            end if
         end if
      else
         delete variable sAERotateGroup[theGroup]
      end if
   end repeat
   
   unlock screen
   put the milliseconds-tElapsed into tElapsed
   if the keys of tAeEasing is not empty OR the keys of tAEEasingCircular is not empty OR the keys of sAEMorphing is not empty OR the keys of sAEMorphGradientRamp is not empty OR the keys of sAERotateGroup is not empty then
      if tElapsed < 1000 / sAEFrameRate then
         if "aeGeneral" is not in the pendingmessages then
            send "aeGeneral" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeGeneral" is not in the pendingmessages then
            send "aeGeneral" to me in 5 millisecs
         end if
      end if
   end if
   --   wait 0 milliseconds with messages
end aeGeneral

on aeMoveDone
   -- trapped to avoid scripterrors
end aeMoveDone

on aeMorphDone
   -- trapped to avoid scripterrors
end aeMorphDone


on aeEnterFrame
   
end aeEnterFrame

on aeExitFrame
   
end aeExitFrame

on aeStopMoving tControl
   if tControl="all" then
      delete variable tAEEasing
   else
      delete variable tAEEasing[the long id of tControl]
   end if
end aeStopMoving

--> 2.9 geometry stuff

/**

Summary: Tests whether a point lies within an ellipse

Returns: The computed result
*/
function aeWithinEllipse
   -- params center of ellipse,xRadius,yRadius,testPoint
   local tX,tY,tRadX,tRadY,tX2,tY2,theValue
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tX
   put item 2 of theValue into tY
   put item 3 of theValue into tRadX
   put item 4 of theValue into tRadY
   put item 5 of theValue into tX2
   put item 6 of theValue into tY2
   if tx is not a number then return "error: x-loc of ellipse is not a number"
   if ty is not a number then return "error: y-loc of ellipse is not a number"
   if tradx is not a number then return "error: x-radius of ellipse is not a number"
   if trady is not a number then return "error: y-radius of ellipse is not a number"
   if tx2 is not a number then return "error: x-loc of point is not a number"
   if ty2 is not a number then return "error: y-loc of point is not a number"
   return (tX2-tX)^2 / tradx^2 + (ty2-ty)^2 / trady^2 <=1
end aeWithinEllipse



--> 6.0 morph pointlists

on aeLockMorph
   put true into sAELockMorph
end aeLockMorph

on aeUnlockMorph
   put false into sAELockMorph
   if "aeGeneral" is not in the pendingMessages then
      aeGeneral
   end if
end aeUnlockMorph

on aeLockMorphGradientRamp
   put true into sAELockMorphGradientRamp
end aeLockMorphGradientRamp

on aeUnlockMorphGradientRamp
   put false into sAELockMorphGradientRamp
   if "aeGeneral" is not in the pendingMessages then
      aeGeneral
   end if
end aeUnlockMorphGradientRamp


on aeMorphGraphic pControl,pPointList1,pPointList2,pDuration,pEffect
   if sAEFrameRate is empty then aeResetFrameRate
   if there is no pControl then return "error:"&&pControl&&"is not a valid control or group or stack"
   if word 1 of the long name of pControl is not "graphic" then return "error: morphing only works for graphics"
   if the number of lines of pPointList1 <> the number of lines of pPointList2 then return "error: pointlists must have equal number of lines"
   if pDuration is not a number then return "error:"&&pDuration&&"is not a valid duration"
   if pEffect is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&pEffect&&"is not a valid easing method. Must be in,out or inOut"
   put pPointList1 into sAEMorphing[the long id of pControl]["p1"]
   put pPointList2 into sAEMorphing[the long id of pControl]["p2"]
   put pDuration into sAEMorphing[the long id of pControl]["duration"]
   put pEffect into sAEMorphing[the long id of pControl]["Effect"]
   if not sAELockMorph then
      put the millisecs into sAEMorphing[the long id of pControl]["startTime"]
      if "aeGeneral" is not in the pendingmessages then
         aeGeneral
      end if
   else
      put "pending" into sAEMorphing[the long id of pControl]["startTime"]
   end if
end aeMorphGraphic

on aeMorphGradientRamp pControl,pPointList1,pPointList2,pDuration,pEffect
   if sAEFrameRate is empty then aeResetFrameRate
   if there is no pControl then return "error:"&&pControl&&"is not a valid control or group or stack"
   if word 1 of the long name of pControl is not "graphic" then return "error: morphing only works for graphics"
   if the number of lines of pPointList1 <> the number of lines of pPointList2 then return "error: pointlists must have equal number of lines"
   if pDuration is not a number then return "error:"&&pDuration&&"is not a valid duration"
   if pEffect is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&pEffect&&"is not a valid easing method. Must be in,out or inOut"
   put pPointList1 into sAEMorphGradientRamp[the long id of pControl]["p1"]
   put pPointList2 into sAEMorphGradientRamp[the long id of pControl]["p2"]
   put pDuration into sAEMorphGradientRamp[the long id of pControl]["duration"]
   put pEffect into sAEMorphGradientRamp[the long id of pControl]["Effect"]
   if not sAELockMorphGradientRamp then
      put the millisecs into sAEMorphGradientRamp[the long id of pControl]["startTime"]
      if "aeGeneral" is not in the pendingmessages then
         aeGeneral
      end if
   else
      put "pending" into sAEMorphGradientRamp[the long id of pControl]["startTime"]
   end if
end aeMorphGradientRamp

on aeMorphGradientDone
   
end aeMorphGradientDone

/**

Summary: Interpolates between two point lists over time

Parameters:
pList1: The first point list
pList2: The second point list
pDuration: The duration of the interpolation
pCurrentTime: The current time in the interpolation
pEffect: The easing effect to use for the interpolation (bounce, overshoot, in, out, inOut)

Returns: The computed result
*/
function aeMorphPointlists pList1,pList2,pDuration,pCurrentTime,pEffect
   local tResultList,tCounter
   if the number of lines of pList1 <> the number of lines of pList2 then return "error: number of points in lists is not equal"
   repeat for each line theLine in pList1
      add 1 to tCounter
      if line tCounter of pList1 is empty or line tCounter of pList2 is empty then
         put cr after tResultList
         next repeat
      end if
      switch pEffect
         case "bounce"
            put aeBounceEaseOut(item 1 of line tCounter of pList1,item 1 of line tCounter of pList2,pDuration,pCurrentTime) & "," & aeBounceEaseOut(item 2 of line tCounter of pList1,item 2 of line tCounter of pList2,pDuration,pCurrentTime) & cr after tResultList
            break
         case "overShoot"
            put aeoverShootEaseOut(item 1 of line tCounter of pList1,item 1 of line tCounter of pList2,pDuration,pCurrentTime) & "," & aeOvershootEaseOut(item 2 of line tCounter of pList1,item 2 of line tCounter of pList2,pDuration,pCurrentTime) & cr after tResultList
            break
         case "in"
            put aeEaseIn(item 1 of line tCounter of pList1,item 1 of line tCounter of pList2,pDuration,pCurrentTime,3) & "," & aeEaseIn(item 2 of line tCounter of pList1,item 2 of line tCounter of pList2,pDuration,pCurrentTime,3) & cr after tResultList
            break
         case "out"
            put aeEaseOut(item 1 of line tCounter of pList1,item 1 of line tCounter of pList2,pDuration,pCurrentTime,3) & "," & aeEaseOut(item 2 of line tCounter of pList1,item 2 of line tCounter of pList2,pDuration,pCurrentTime,3) & cr after tResultList
            break
         case "inOut"
            put aeEaseInOut(item 1 of line tCounter of pList1,item 1 of line tCounter of pList2,pDuration,pCurrentTime,3) & "," & aeEaseInOut(item 2 of line tCounter of pList1,item 2 of line tCounter of pList2,pDuration,pCurrentTime,3) & cr after tResultList
            break
         default
            put aeEaseIn(item 1 of line tCounter of pList1,item 1 of line tCounter of pList2,pDuration,pCurrentTime,2) & "," & aeEaseIn(item 2 of line tCounter of pList1,item 2 of line tCounter of pList2,pDuration,pCurrentTime,2) & cr after tResultList
            break
      end switch
   end repeat
   delete char -1 of tResultList
   return tResultList
end aeMorphPointlists

/**

Summary: Interpolates between two numeric lists over time

Parameters:
pList1: The first numeric list
pList2: The second numeric list
pDuration: The duration of the interpolation
pCurrentTime: The current time in the interpolation
pEffect: The easing effect to use for the interpolation (bounce, overshoot, in, out, inOut)
pRound: Boolean indicating whether to round the result
pMin: Minimum value for the result (optional)
pMax: Maximum value for the result (optional)

Returns: The computed result
*/
function aeMorphLists pList1,pList2,pDuration,pCurrentTime,pEffect,pRound,pMin,pMax
   local tResultList,tCounter,tVal
   if the number of lines of pList1 <> the number of lines of pList2 then return "error: number of lines in lists is not equal"
   repeat for each line theLine in pList1
      add 1 to tCounter
      if line tCounter of pList1 is empty or line tCounter of pList2 is empty then
         put cr after tResultList
         next repeat
      end if
      if the number of items of  line tCounter of pList1<> the number of items of line tCounter of pList2 then return "error: number of items in lists is not equal"
      repeat with i=1 to the number of items of theLine
         switch pEffect
            case "bounce"
               put aeBounceEaseOut(item i of line tCounter of pList1,item i of line tCounter of pList2,pDuration,pCurrentTime) into tVal 
               break
            case "overShoot"
               put aeoverShootEaseOut(item i of line tCounter of pList1,item i of line tCounter of pList2,pDuration,pCurrentTime) into tVal
               break
            case "in"
               put aeEaseIn(item i of line tCounter of pList1,item i of line tCounter of pList2,pDuration,pCurrentTime,3) into tVal
               break
            case "out"
               put aeEaseOut(item i of line tCounter of pList1,item i of line tCounter of pList2,pDuration,pCurrentTime,3) into tVal
               break
            case "inOut"
               put aeEaseInOut(item i of line tCounter of pList1,item i of line tCounter of pList2,pDuration,pCurrentTime,3) into tVal
               break
            default
               put aeEaseIn(item i of line tCounter of pList1,item i of line tCounter of pList2,pDuration,pCurrentTime,2) into tVal
               break
         end switch
         if pRound is true then put round(tVal) into tVal
         if pMin is not empty then
            if tVal < pMin then 
               put pMin into tVal
            end if
         end if
         if pMax is not empty then
            if tVal > pMax then 
               put pMax into tVal
            end if
         end if
         put tVal & "," after tResultList
      end repeat
      put cr into char -1 of tResultList
   end repeat
   delete char -1 of tResultList
   return tResultList
end aeMorphLists


/**

Summary: Interpolates between two gradient ramp lists over time

Parameters:
pList1: The first gradient ramp list
pList2: The second gradient ramp list
pDuration: The duration of the interpolation
pCurrentTime: The current time in the interpolation
pEffect: The easing effect to use for the interpolation (bounce, overshoot, in, out, inOut)

Returns: The computed result
*/
function aeMorphGradient pList1,pList2,pDuration,pCurrentTime,pEffect
   --answer pDuration & cr & pCurrenttime
   local tResultList,tCounter,tVal
   if the number of lines of pList1 <> the number of lines of pList2 then return "error: number of lines in lists is not equal"
   repeat for each line theLine in pList1
      add 1 to tCounter
      if line tCounter of pList1 is empty or line tCounter of pList2 is empty then
         put cr after tResultList
         next repeat
      end if
      if the number of items of  line tCounter of pList1<> the number of items of line tCounter of pList2 then return "error: number of items in lists is not equal"
      repeat with i=1 to the number of items of theLine
         switch pEffect
            case "bounce"
               put aeBounceEaseOut(item i of line tCounter of pList1,item i of line tCounter of pList2,pDuration,pCurrentTime) into tVal 
               break
            case "overShoot"
               put aeoverShootEaseOut(item i of line tCounter of pList1,item i of line tCounter of pList2,pDuration,pCurrentTime) into tVal
               break
            case "in"
               put aeEaseIn(item i of line tCounter of pList1,item i of line tCounter of pList2,pDuration,pCurrentTime,3) into tVal
               break
            case "out"
               put aeEaseOut(item i of line tCounter of pList1,item i of line tCounter of pList2,pDuration,pCurrentTime,3) into tVal
               break
            case "inOut"
               put aeEaseInOut(item i of line tCounter of pList1,item i of line tCounter of pList2,pDuration,pCurrentTime,3) into tVal
               break
            default
               put aeEaseIn(item i of line tCounter of pList1,item i of line tCounter of pList2,pDuration,pCurrentTime,2) into tVal
               break
         end switch
         if i=1 then
            if tVal > 1 then put 1 into tVal
            if tVal <0 then put 0 into tVal
         end if
         if i>1 then
            put round(tVal) into tVal
            if tVal > 255 then put 255 into tVal
            if tVal <0 then put 0 into tVal
         end if
         put tVal & "," after tResultList
      end repeat
      put cr into char -1 of tResultList
   end repeat
   delete char -1 of tResultList
   return tResultList
end aeMorphGradient

--> 2.9 easing stuff

/**

Summary: Returns a bounce ease-in interpolated value

Parameters:
pStart: The starting value
pEnd: The ending value
pDuration: The duration of the interpolation
pCurrentTime: The current time in the interpolation

Returns: The computed result
*/
function aeBounceEaseIn pStart,pEnd,pDuration,pCurrentTime
   if pStart is not a number then return "error: start is not a number"
   if pEnd is not a number then return "error: end is not a number"
   if pDuration is not a number then return "error: duration is not a number"
   if pDuration < 0 then return "error: duration is < 0"
   if pCurrentTime is not a number then return "error: currenttime is not a number"
   return pEnd - aeBounceEaseOut(pDuration - pCurrentTime,0,pEnd,pDuration) + pStart
end aeBounceEaseIn

/**

Summary: Performs the aeBounceEaseOut operation

Parameters:
pStart: The starting value
pEnd: The ending value
pDuration: The duration of the interpolation
pCurrentTime: The current time in the interpolation

Returns: The computed result
*/
function aeBounceEaseOut pStart,pEnd,pDuration,pCurrentTime
   local tValue
   if pStart is not a number then return "error: start is not a number"
   if pEnd is not a number then return "error: end is not a number"
   if pDuration is not a number then return "error: duration is not a number"
   if pDuration < 0 then return "error: duration is < 0"
   if pCurrentTime is not a number then return "error: currenttime is not a number"
   put pEnd-pStart into tValue
   if pCurrentTime>pDuration then put pDuration into pCurrentTime
   if pCurrentTime/pDuration < 1/2.75 then
      return tValue *(7.56525*(pCurrentTime/pDuration)^2) +pStart-- & " if 1"
   else
      if pCurrentTime/pDuration < 2/2.75 then
         
         return tValue *(7.5625*((pCurrentTime/pDuration)-(1.5/2.75))*((pCurrentTime/pDuration)-(1.5/2.75))+0.75) + pStart-- & " if 2"
      else
         if pCurrentTime/pDuration < 2.5/2.75 then
            
            return tValue*(7.5625*((pCurrentTime/pDuration)-(2.25/2.75))*((pCurrentTime/pDuration) - (2.25/2.75)) + 0.9375) +pStart-- & " if 3"
         else
            return tValue*(7.5625*((pCurrentTime/pDuration)-(2.625/2.75))*((pCurrentTime/pDuration) -(2.625/2.75))  + 0.984375) +pStart --& " if 4"
         end if
      end if
   end if
end aeBounceEaseOut

/**

Summary: Returns an overshoot ease-in interpolated value

Parameters:
pStart: The starting value
pEnd: The ending value
pDuration: The duration of the interpolation
pCurrentTime: The current time in the interpolation
pOvershoot: The amount of overshoot

Returns: The computed result
*/
function aeOvershootEaseIn pStart,pEnd,pDuration,pCurrentTime,pOvershoot
   if pStart is not a number then return "error: start is not a number"
   if pEnd is not a number then return "error: end is not a number"
   if pDuration is not a number then return "error: duration is not a number"
   if pDuration < 0 then return "error: duration is < 0"
   if pOverShoot is not a number then return "error: overshoot is not a number"
   if pOvershoot is empty then put 1.70158 into pOvershoot
   
   if pCurrentTime is not a number then return "error: currenttime is not a number"
   return (pEnd - pStart)*(pCurrentTime/pDuration)*(pcurrentTime/pDuration)*((pOvershoot+1)*(pcurrenttime/pDuration)-pOvershoot)+pStart
end aeOvershootEaseIn

/**

Summary: Performs the aeOverShootEaseOut operation

Parameters:
pStart: The starting value
pEnd: The ending value
pDuration: The duration of the interpolation
pCurrentTime: The current time in the interpolation
pOvershoot: The amount of overshoot

Returns: The computed result
*/
function aeOverShootEaseOut pStart,pEnd,pDuration,pCurrentTime,pOvershoot
   if pOvershoot is empty then put 1.70158 into pOvershoot
   if pStart is not a number then return "error: start is not a number"
   if pEnd is not a number then return "error: end is not a number"
   if pDuration is not a number then return "error: duration is not a number"
   if pDuration < 0 then return "error: duration is < 0"
   if pOverShoot is not a number then return "error: overshoot is not a number"
   if pCurrentTime is not a number then return "error: currenttime is not a number"
   if pOvershoot is empty then put 1.70158 into pOvershoot
   return (pEnd - pStart)*((pCurrentTime/pDuration-1)*(pCurrentTime/pDuration-1)*((pOverShoot+1)*(pCurrentTime/pDuration-1)+pOvershoot)+1)+pStart
end aeOverShootEaseOut

/**

Summary: Logs a debug value to the animation engine debug output

Parameters:
pValue: The value to log
*/
command aeDebug pValue
   put cr&pValue after msg
end aeDebug

--> 3.0 update

on aeRotateGroup
   local tControl,tX,tY,tDuration,tMethod,theValue,tDistance,tAngle
   if sAEFrameRate is empty then aeResetFrameRate
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tControl
   put item 2 of theValue into tX
   put item 3 of theValue into tY
   put item 4 of theValue into tAngle
   put item 5 of theValue into tDuration
   put item 6 of theValue into tMethod
   
   repeat with i=1 to the number of controls of tControl
      if the name of control i of tControl begins with "group" then next repeat
      put distance(tx,ty,the loc of control i of tControl) into sAERotateGroup[tControl]["controls"][the long ID of control i of tControl]["Distance"]
      put findPreciseAngleX(tX,tY,the loc of control i of tControl) into sAERotateGroup[tControl]["controls"][the long ID of control i of tControl]["Angle"]
   end repeat
   put tX,tY into sAERotateGroup[tControl]["center"]
   put tMethod into sAERotateGroup[tControl]["Effect"]
   put tDuration into sAERotateGroup[tControl]["Duration"]
   put tAngle into sAERotateGroup[tControl]["Angle"]
   
   if not sAELockRotateGroup then
      put the milliSecs into sAERotateGroup[tControl]["StartTime"]
      if "aeGeneral" is not in the pendingmessages then
         aeGeneral
      end if
   else
      put "pending" into sAERotateGroup[tControl]["StartTime"]
   end if
end aeRotateGroup

local moof

on aeCrossfade pControl1,pControl2,pDuration
   local tTarget
   if there is no pControl1 then return "aeError: "&pControl1&&"is not a valid control reference"
   if there is no pControl2 then return "aeError: "&pControl2&&"is not a valid control reference"
   if pDuration is not a number then return "aeError: Duration is not a number"
   set the blendlevel of pControl1 to 100
   set the blendlevel of pControl2 to 0
   set the visible of pControl1 to true
   set the visible of pControl2 to true
   replace quote with "§$aeQuote$§" in pControl1
   replace quote with "§$aeQuote$§" in pControl2
   put the long id of the target into tTarget
   replace quote with "§$aeQuote$§" in tTarget
   _aeCrossfade pControl1,pControl2,pDuration,the millisecs,tTarget
   put 0 into moof
end aeCrossfade

on _aeCrossfade pControl1,pControl2,pDuration,pStart,pTarget
   local tControl1,tControl2,tTarget
   put pControl1 into tControl1
   put pControl2 into tControl2
   replace  "§$aeQuote$§" with quote in tControl1
   replace  "§$aeQuote$§" with quote in tControl2
   local tElapsed,tValue
   put the millisecs - pStart into tElapsed
   put round(aeEaseIn(0,100,pDuration,tElapsed,1)) into tValue
   set the blendlevel of tControl1 to 100-tValue
   set the blendlevel of tControl2 to tValue
   if tElapsed<pDuration then
      send "_aeCrossfade" && pControl1,pControl2,pDuration,pStart,pTarget to me in 25 millisecs
   else
      set the visible of tControl2 to false
      set the blendlevel of tControl2 to 0
      put pTarget into tTarget
      replace  "§$aeQuote$§" with quote in tTarget
      send "aeCrossfadeDone" to tTarget
   end if
end _aeCrossfade

on aeCrossfadeDone
   -- trapped to avoid errors
end aeCrossfadeDone

on aeFadeIn pControl,pDuration
   local tTarget
   if there is no pControl then return "aeError: "&pControl&&"is not a valid control reference"
   if pDuration is not a number then return "aeError: Duration is not a number"
   set the blendlevel of pControl to 100
   set the visible of pControl to true
   replace quote with "§$aeQuote$§" in pControl
   put the long id of the target into tTarget
   replace quote with "§$aeQuote$§" in tTarget
   _aeFadeIn pControl,pDuration,the millisecs,tTarget
end aeFadeIn

on _aeFadeIn pControl,pDuration,pStart,pTarget
   local tControl,tTarget
   put pControl into tControl
   replace  "§$aeQuote$§" with quote in tControl
   local tElapsed,tValue
   put the millisecs - pStart into tElapsed
   put round(aeEaseIn(0,100,pDuration,tElapsed,1)) into tValue
   set the blendlevel of tControl to 100-tValue
   if tElapsed<pDuration then
      send "_aeFadeIn" && pControl,pDuration,pStart,pTarget to me in 25 millisecs
   else
      put pTarget into tTarget
      replace  "§$aeQuote$§" with quote in tTarget
      send "aeFadeInDone" to tTarget
   end if
end _aeFadeIn

on aeFadeInDone
   -- trapped to avoid errors
end aeFadeInDone

on aeFadeOut pControl,pDuration
   local tTarget
   if there is no pControl then return "aeError: "&pControl&&"is not a valid control reference"
   if pDuration is not a number then return "aeError: Duration is not a number"
   set the blendlevel of pControl to 0
   set the visible of pControl to true
   replace quote with "§$aeQuote$§" in pControl
   put the long id of the target into tTarget
   replace quote with "§$aeQuote$§" in tTarget
   _aeFadeOut pControl,pDuration,the millisecs,tTarget
end aeFadeOut

on _aeFadeOut pControl,pDuration,pStart,pTarget
   local tControl,tTarget
   put pControl into tControl
   replace  "§$aeQuote$§" with quote in tControl
   local tElapsed,tValue
   put the millisecs - pStart into tElapsed
   put round(aeEaseIn(0,100,pDuration,tElapsed,1)) into tValue
   set the blendlevel of tControl to tValue
   if tElapsed<pDuration then
      send "_aeFadeOut" && pControl,pDuration,pStart,pTarget to me in 25 millisecs
   else
      put pTarget into tTarget
      replace  "§$aeQuote$§" with quote in tTarget
      send "aeFadeOutDone" to tTarget
      hide tControl
      set the blendlevel of tControl  to 0
   end if
end _aeFadeOut

on aeFadeOutDone
   -- trapped to avoid errors
end aeFadeOutDone

local sAELockRectChanging,tAeChangeRect
local sAELockScrolls,tAeScrolling

on aeChangeRect
   local tControl,tLeft,tRight,tTop,tBottom,tDuration,tMethod,theValue
   if sAEFrameRate is empty then aeResetFrameRate
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tControl
   put item 2 of theValue into tLeft
   put item 3 of theValue into tTop
   put item 4 of theValue into tRight
   put item 5 of theValue into tBottom
   put item 6 of theValue into tDuration
   put item 7 of theValue into tMethod
   if there is no tControl then return "error:"&&tControl&&"is not a valid control or group or stack"
   aeStopChangingWidth tControl
   aeStopChangingHeight tControl
   if word 1 of the long name of tControl is "card" then return "error: can not resize a card"
   if tLeft is not an integer then return "error:"&&tLeft&","&tTop&","&tRight&";"&tBottom&&"is not a valid rectangle"
   if tTop is not an integer then return "error:"&&tLeft&","&tTop&","&tRight&";"&tBottom&&"is not a valid rectangle"
   if tRight is not an integer then return "error:"&&tLeft&","&tTop&","&tRight&";"&tBottom&&"is not a valid rectangle"
   if tBottom is not an integer then return "error:"&&tLeft&","&tTop&","&tRight&";"&tBottom&&"is not a valid rectangle"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tMethod&&"is not a valid easing method. Must be in,out or inOut"
   if not sAELockRectChanging then
      put tLeft,tTop,tRight,tBottom,tDuration,tMethod,item 1 of the rect of tControl,item 2 of the rect of tControl,item 3 of the rect of tControl,item 4 of the rect of tControl,the milliseconds into tAeChangeRect[the long id of tControl]
      if "aeChangingRect" is not in the pendingmessages then
         aeChangingRect
      end if
   else
      put tLeft,tTop,tRight,tBottom,tDuration,tMethod,item 1 of the rect of tControl,item 2 of the rect of tControl,item 3 of the rect of tControl,item 4 of the rect of tControl,"pending" into tAeChangeRect[the long id of tControl]
   end if
end aeChangeRect

on aeLockRects
   put true into sAELockRectChanging
end aeLockRects

on aeUnlockRects
   local tNow
   put the milliseconds into tNow
   repeat for each line pControl in the keys of tAeChangeRect
      if item 11 of tAeChangeRect[pControl] <> "pending" then next repeat
      put tNow into item 11 of tAeChangeRect[pControl]
   end repeat
   put false into sAELockRectChanging
   if "aeChangingRect" is not in the pendingmessages then aeChangingRect
end aeUnlockRects

local sTimeTakenToChangeRects

/**

Summary: Animates the rect property of the target object
*/
command aeChangingRect
   lock screen
   local tTimeLost,tFramesLost,tElapsed
   local tLeft,tTop,tRight,tBottom,tDuration,tMethod,tEndX,tStartLeft,tDestLeft,tStartTop,tDestTop,tStartRight,tDestRight,tStartBottom,tDestBottom,tStartTime
   if sAEFrameRate is not a number then aeResetFrameRate
   put the milliseconds into tElapsed
   put the milliseconds - sTimeTakenToChangeRects into tTimeLost
   put tTimeLost - (1000/ sAEFrameRate) into tTimeLost
   put ((1000/sAeFramerate) + tTimeLost) into tFramesLost
   if tFramesLost<>0 then
      put 1000/tFramesLost into tFramesLost
   end if
   if tFramesLost>sAEFrameRate then put sAEFrameRate into tFramesLost
   put tFramesLost into sAERealFrameRate
   put the millisecs into sTimeTakenToChangeRects
   --lock screen
   repeat for each line pControl in the keys of tAeChangeRect
      send "aeEnterFrame" && "aeChangeRect" to pcontrol
      put item 1 of tAeChangeRect[pControl] into tDestLeft
      put item 2 of tAeChangeRect[pControl] into tDestTop
      put item 3 of tAeChangeRect[pControl] into tDestRight
      put item 4 of tAeChangeRect[pControl] into tDestBottom
      put item 5 of tAeChangeRect[pControl] into tDuration
      put item 6 of tAeChangeRect[pControl] into tMethod
      put item 7 of tAeChangeRect[pControl] into tStartLeft
      put item 8 of tAeChangeRect[pControl] into tStartTop
      put item 9 of tAeChangeRect[pControl] into tStartRight
      put item 10 of tAeChangeRect[pControl] into tStartBottom
      put item 11 of tAeChangeRect[pControl] into tStartTime
      if tStartTime="pending" then next repeat
      switch tMethod
         case "in"
            put aeEaseIn(tStartLeft,tDestLeft,tDuration,the milliseconds-tStartTime,3) into tLeft
            put aeEaseIn(tStartTop,tDestTop,tDuration,the milliseconds-tStartTime,3) into tTop
            put aeEaseIn(tStartRight,tDestRight,tDuration,the milliseconds-tStartTime,3) into tRight
            put aeEaseIn(tStartBottom,tDestBottom,tDuration,the milliseconds-tStartTime,3) into tBottom
            break
         case "out"
            put aeEaseOut(tStartLeft,tDestLeft,tDuration,the milliseconds-tStartTime,3) into tLeft
            put aeEaseOut(tStartTop,tDestTop,tDuration,the milliseconds-tStartTime,3) into tTop
            put aeEaseOut(tStartRight,tDestRight,tDuration,the milliseconds-tStartTime,3) into tRight
            put aeEaseOut(tStartBottom,tDestBottom,tDuration,the milliseconds-tStartTime,3) into tBottom
            break
         case "inOut"
            put aeEaseInOut(tStartLeft,tDestLeft,tDuration,the milliseconds-tStartTime,3) into tLeft
            put aeEaseInOut(tStartTop,tDestTop,tDuration,the milliseconds-tStartTime,3) into tTop
            put aeEaseInOut(tStartRight,tDestRight,tDuration,the milliseconds-tStartTime,3) into tRight
            put aeEaseInOut(tStartBottom,tDestBottom,tDuration,the milliseconds-tStartTime,3) into tBottom
            break
         case "bounce"
            put aeBounceEaseOut(tStartLeft,tDestLeft,tDuration,the milliseconds-tStartTime) into tLeft
            put aeBounceEaseOut(tStartTop,tDestTop,tDuration,the milliseconds-tStartTime) into tTop
            put aeBounceEaseOut(tStartRight,tDestRight,tDuration,the milliseconds-tStartTime) into tRight
            put aeBounceEaseOut(tStartBottom,tDestBottom,tDuration,the milliseconds-tStartTime) into tBottom
            break
         case "overshoot"
            put aeOverShootEaseOut(tStartLeft,tDestLeft,tDuration,the milliseconds-tStartTime) into tLeft
            put aeOverShootEaseOut(tStartTop,tDestTop,tDuration,the milliseconds-tStartTime) into tTop
            put aeOverShootEaseOut(tStartRight,tDestRight,tDuration,the milliseconds-tStartTime) into tRight
            put aeOverShootEaseOut(tStartBottom,tDestBottom,tDuration,the milliseconds-tStartTime) into tBottom
            break
         default
            put aeEaseIn(tStartLeft,tDestLeft,tDuration,the milliseconds-tStartTime,1) into tLeft
            put aeEaseIn(tStartTop,tDestTop,tDuration,the milliseconds-tStartTime,1) into tTop
            put aeEaseIn(tStartRight,tDestRight,tDuration,the milliseconds-tStartTime,1) into tRight
            put aeEaseIn(tStartBottom,tDestBottom,tDuration,the milliseconds-tStartTime,1) into tBottom
            break
      end switch
      if the milliseconds-tStartTime<tDuration then
         set the rect of pControl to tLeft,tTop,tRight,tBottom
      else
         if tDestLeft,tDestTop,tDestRight,tDestBottom is a rectangle then
            set the rect of pControl to tDestLeft,tDestTop,tDestRight,tDestBottom
         end if
         delete variable tAeChangeRect[pControl]
         send "aeChangeRectDone" to pControl
      end if
      send "aeExitFrame" && "aeChangeRect" to pControl
   end repeat
   --   unlock screen
   unlock screen
   put the milliseconds-tElapsed into tElapsed
   if the keys of tAeChangeRect is not empty then
      if tElapsed < 1000 / sAEFrameRate then
         if "aeChangingRect" is not in the pendingmessages then
            send "aeChangingRect" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeChangingRect" is not in the pendingmessages then
            send "aeChangingRect" to me in 5 millisecs
         end if
      end if
   end if
   --   wait 0 milliseconds with messages
   --put "changingrect" && the millisecs
end aeChangingRect

on aeChangeRectDone
   
end aeChangeRectDone


local sAELockHeightChanging,tAeChangeHeight

on aeChangeHeight
   local tControl,tHeight,tDuration,tMethod,theValue
   if sAEFrameRate is empty then aeResetFrameRate
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tControl
   put item 2 of theValue into tHeight
   put item 3 of theValue into tDuration
   put item 4 of theValue into tMethod
   if there is no tControl then return "error:"&&tControl&&"is not a valid control or group or stack"
   if word 1 of the long name of tControl is "card" then return "error: can not resize a card"
   if tHeight is not an integer then return "error:"&&tHeight&&"is not a valid height"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tMethod&&"is not a valid easing method. Must be in,out or inOut"
   aeStopChangingRect tControl
   
   if not sAELockHeightChanging then
      put tHeight,tDuration,tMethod,the height of tControl,the milliseconds into tAeChangeHeight[the long id of tControl]
      if "aeChangingHeight" is not in the pendingmessages then
         aeChangingHeight
      end if
   else
      put tHeight,tDuration,tMethod,the height of tControl,"pending" into tAeChangeHeight[the long id of tControl]
   end if
end aeChangeHeight

on aeLockHeights
   put true into sAELockHeightChanging
end aeLockHeights

on aeUnlockHeights
   local tNow
   put the milliseconds into tNow
   repeat for each line pControl in the keys of tAeChangeHeight
      if item 5 of tAeChangeHeight[pControl] <> "pending" then next repeat
      put tNow into item 5 of tAeChangeHeight[pControl]
   end repeat
   put false into sAELockHeightChanging
   if "aeChangingHeight" is not in the pendingmessages then aeChangingHeight
end aeUnlockHeights

local sTimeTakenToChangeHeights

/**

Summary: Animates the height property of the target object
*/
command aeChangingHeight
   lock screen
   local tTimeLost,tFramesLost,tElapsed
   local tHeight,tDuration,tMethod,tStartHeight,tDestHeight,tStartTime
   if sAEFrameRate is not a number then aeResetFrameRate
   put the milliseconds into tElapsed
   put the milliseconds - sTimeTakenToChangeHeights into tTimeLost
   put tTimeLost - (1000/ sAEFrameRate) into tTimeLost
   put ((1000/sAeFramerate) + tTimeLost) into tFramesLost
   if tFramesLost<>0 then
      put 1000/tFramesLost into tFramesLost
   end if
   if tFramesLost>sAEFrameRate then put sAEFrameRate into tFramesLost
   put tFramesLost into sAERealFrameRate
   put the millisecs into sTimeTakenToChangeHeights
   --lock screen
   repeat for each line pControl in the keys of tAeChangeHeight
      send "aeEnterFrame" &&"aeChangeHeight" to pcontrol
      put item 1 of tAeChangeHeight[pControl] into tDestHeight
      put item 2 of tAeChangeHeight[pControl] into tDuration
      put item 3 of tAeChangeHeight[pControl] into tMethod
      put item 4 of tAeChangeHeight[pControl] into tStartHeight
      put item 5 of tAeChangeHeight[pControl] into tStartTime
      if tStartTime="pending" then next repeat
      switch tMethod
         case "in"
            put aeEaseIn(tStartHeight,tDestHeight,tDuration,the milliseconds-tStartTime,3) into tHeight
            break
         case "out"
            put aeEaseOut(tStartHeight,tDestHeight,tDuration,the milliseconds-tStartTime,3) into tHeight
            break
         case "inOut"
            put aeEaseInOut(tStartHeight,tDestHeight,tDuration,the milliseconds-tStartTime,3) into tHeight
            break
         case "bounce"
            put aeBounceEaseOut(tStartHeight,tDestHeight,tDuration,the milliseconds-tStartTime) into tHeight
            break
         case "overshoot"
            put aeOverShootEaseOut(tStartHeight,tDestHeight,tDuration,the milliseconds-tStartTime) into tHeight
            break
         default
            put aeEaseIn(tStartHeight,tDestHeight,tDuration,the milliseconds-tStartTime,1) into tHeight
            break
      end switch
      if the milliseconds-tStartTime<tDuration then
         set the height of pControl to tHeight
      else
         if tHeight is a number then
            set the height of pControl to tHeight
         end if
         delete variable tAeChangeHeight[pControl]
         send "aeChangeHeightDone" to pControl
      end if
      send "aeExitFrame" &&"aeChangeHeight" to pControl
   end repeat
   unlock screen
   put the milliseconds-tElapsed into tElapsed
   if the keys of tAeChangeHeight is not empty then
      if tElapsed < 1000 / sAEFrameRate then
         if "aeChangingHeight" is not in the pendingmessages then
            send "aeChangingHeight" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeChangingHeight" is not in the pendingmessages then
            send "aeChangingHeight" to me in 5 millisecs
         end if
      end if
   end if
end aeChangingHeight

on aeChangeHeightDone
   
end aeChangeHeightDone

local sAELockWidthChanging,tAeChangeWidth

on aeChangeWidth
   local tControl,tWidth,tDuration,tMethod,theValue
   if sAEFrameRate is empty then aeResetFrameRate
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tControl
   put item 2 of theValue into tWidth
   put item 3 of theValue into tDuration
   put item 4 of theValue into tMethod
   if there is no tControl then return "error:"&&tControl&&"is not a valid control or group or stack"
   if word 1 of the long name of tControl is "card" then return "error: can not resize a card"
   if tWidth is not an integer then return "error:"&&tWidth&&"is not a valid Width"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tMethod&&"is not a valid easing method. Must be in,out or inOut"
   aeStopChangingRect tControl
   
   if not sAELockWidthChanging then
      put tWidth,tDuration,tMethod,the width of tControl,the milliseconds into tAeChangeWidth[the long id of tControl]
      if "aeChangingWidth" is not in the pendingmessages then
         aeChangingWidth
      end if
   else
      put tWidth,tDuration,tMethod,the width of tControl,"pending" into tAeChangeWidth[the long id of tControl]
   end if
end aeChangeWidth

on aeLockWidths
   put true into sAELockWidthChanging
end aeLockWidths

on aeUnlockWidths
   local tNow
   put the milliseconds into tNow
   repeat for each line pControl in the keys of tAeChangeWidth
      if item 5 of tAeChangeWidth[pControl] <> "pending" then next repeat
      put tNow into item 5 of tAeChangeWidth[pControl]
   end repeat
   put false into sAELockWidthChanging
   if "aeChangingWidth" is not in the pendingmessages then aeChangingWidth
end aeUnlockWidths

local sTimeTakenToChangeWidths

/**

Summary: Animates the width property of the target object
*/
command aeChangingWidth
   lock screen
   local tTimeLost,tFramesLost,tElapsed
   local tWidth,tDuration,tMethod,tStartWidth,tDestWidth,tStartTime
   if sAEFrameRate is not a number then aeResetFrameRate
   put the milliseconds into tElapsed
   put the milliseconds - sTimeTakenToChangeWidths into tTimeLost
   put tTimeLost - (1000/ sAEFrameRate) into tTimeLost
   put ((1000/sAeFramerate) + tTimeLost) into tFramesLost
   if tFramesLost<>0 then
      put 1000/tFramesLost into tFramesLost
   end if
   if tFramesLost>sAEFrameRate then put sAEFrameRate into tFramesLost
   put tFramesLost into sAERealFrameRate
   put the millisecs into sTimeTakenToChangeWidths
   --lock screen
   repeat for each line pControl in the keys of tAeChangeWidth
      send "aeEnterFrame" &&"aeChangeWidth" to pcontrol
      put item 1 of tAeChangeWidth[pControl] into tDestWidth
      put item 2 of tAeChangeWidth[pControl] into tDuration
      put item 3 of tAeChangeWidth[pControl] into tMethod
      put item 4 of tAeChangeWidth[pControl] into tStartWidth
      put item 5 of tAeChangeWidth[pControl] into tStartTime
      if tStartTime="pending" then next repeat
      switch tMethod
         case "in"
            put aeEaseIn(tStartWidth,tDestWidth,tDuration,the milliseconds-tStartTime,3) into tWidth
            break
         case "out"
            put aeEaseOut(tStartWidth,tDestWidth,tDuration,the milliseconds-tStartTime,3) into tWidth
            break
         case "inOut"
            put aeEaseInOut(tStartWidth,tDestWidth,tDuration,the milliseconds-tStartTime,3) into tWidth
            break
         case "bounce"
            put aeBounceEaseOut(tStartWidth,tDestWidth,tDuration,the milliseconds-tStartTime) into tWidth
            break
         case "overshoot"
            put aeOverShootEaseOut(tStartWidth,tDestWidth,tDuration,the milliseconds-tStartTime) into tWidth
            break
         default
            put aeEaseIn(tStartWidth,tDestWidth,tDuration,the milliseconds-tStartTime,1) into tWidth
            break
      end switch
      if the milliseconds-tStartTime<tDuration then
         set the width of pControl to tWidth
      else
         if tWidth is a number then
            set the width of pControl to tWidth
         end if
         delete variable tAeChangeWidth[pControl]
         send "aeChangeWidthDone" to pControl
      end if
      send "aeExitFrame" &&"aeChangeWidth" to pControl
   end repeat
   unlock screen
   put the milliseconds-tElapsed into tElapsed
   if the keys of tAeChangeWidth is not empty then
      if tElapsed < 1000 / sAEFrameRate then
         if "aeChangingWidth" is not in the pendingmessages then
            send "aeChangingWidth" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeChangingWidth" is not in the pendingmessages then
            send "aeChangingWidth" to me in 5 millisecs
         end if
      end if
   end if
end aeChangingWidth

on aeChangeWidthDone
   
end aeChangeWidthDone

local sAELockHScrollChanging,tAeChangeHScroll

on aeChangeHScroll
   local tControl,tHScroll,tDuration,tMethod,theValue,tType
   if sAEFrameRate is empty then aeResetFrameRate
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tControl
   put item 2 of theValue into tHScroll
   put item 3 of theValue into tDuration
   put item 4 of theValue into tMethod
   if there is no tControl then return "error:"&&tControl&&"is not a valid control or group or stack"
   put word 1 of the long name of tControl into tType
   if word 1 of the long name of tControl is not among the items of "group,field" then return "error: can not scroll a"&&tType
   if tHScroll is not an integer then return "error:"&&tHScroll&&"is not a valid HScroll"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tMethod&&"is not a valid easing method. Must be in,out or inOut"
   if not sAELockHScrollChanging then
      put tHScroll,tDuration,tMethod,the hscroll of tControl,the milliseconds into tAeChangeHScroll[the long id of tControl]
      if "aeChangingHScroll" is not in the pendingmessages then
         aeChangingHScroll
      end if
   else
      put tHScroll,tDuration,tMethod,the hscroll of tControl,"pending" into tAeChangeHScroll[the long id of tControl]
   end if
end aeChangeHScroll

on aeLockHScrolls
   put true into sAELockHScrollChanging
end aeLockHScrolls

on aeUnlockHScrolls
   local tNow
   put the milliseconds into tNow
   repeat for each line pControl in the keys of tAeChangeHScroll
      if item 5 of tAeChangeHScroll[pControl] <> "pending" then next repeat
      put tNow into item 5 of tAeChangeHScroll[pControl]
   end repeat
   put false into sAELockHScrollChanging
   if "aeChangingHScroll" is not in the pendingmessages then aeChangingHScroll
end aeUnlockHScrolls

local sTimeTakenToChangeHScrolls

/**

Summary: Animates the hscroll property of the target object
*/
command aeChangingHScroll
   lock screen
   local tTimeLost,tFramesLost,tElapsed
   local tHScroll,tDuration,tMethod,tStartHScroll,tDestHScroll,tStartTime
   if sAEFrameRate is not a number then aeResetFrameRate
   put the milliseconds into tElapsed
   put the milliseconds - sTimeTakenToChangeHScrolls into tTimeLost
   put tTimeLost - (1000/ sAEFrameRate) into tTimeLost
   put ((1000/sAeFramerate) + tTimeLost) into tFramesLost
   if tFramesLost<>0 then
      put 1000/tFramesLost into tFramesLost
   end if
   if tFramesLost>sAEFrameRate then put sAEFrameRate into tFramesLost
   put tFramesLost into sAERealFrameRate
   put the millisecs into sTimeTakenToChangeHScrolls
   --lock screen
   repeat for each line pControl in the keys of tAeChangeHScroll
      send "aeEnterFrame" &&"aeChangeHScroll" to pcontrol
      put item 1 of tAeChangeHScroll[pControl] into tDestHScroll
      put item 2 of tAeChangeHScroll[pControl] into tDuration
      put item 3 of tAeChangeHScroll[pControl] into tMethod
      put item 4 of tAeChangeHScroll[pControl] into tStartHScroll
      put item 5 of tAeChangeHScroll[pControl] into tStartTime
      if tStartTime="pending" then next repeat
      switch tMethod
         case "in"
            put aeEaseIn(tStartHScroll,tDestHScroll,tDuration,the milliseconds-tStartTime,3) into tHScroll
            break
         case "out"
            put aeEaseOut(tStartHScroll,tDestHScroll,tDuration,the milliseconds-tStartTime,3) into tHScroll
            break
         case "inOut"
            put aeEaseInOut(tStartHScroll,tDestHScroll,tDuration,the milliseconds-tStartTime,3) into tHScroll
            break
         case "bounce"
            put aeBounceEaseOut(tStartHScroll,tDestHScroll,tDuration,the milliseconds-tStartTime) into tHScroll
            break
         case "overshoot"
            put aeOverShootEaseOut(tStartHScroll,tDestHScroll,tDuration,the milliseconds-tStartTime) into tHScroll
            break
         default
            put aeEaseIn(tStartHScroll,tDestHScroll,tDuration,the milliseconds-tStartTime,1) into tHScroll
            break
      end switch
      put round(tHScroll) into tHScroll
      if the milliseconds-tStartTime<tDuration then         
         set the hscroll of pControl to tHScroll
      else
         if tHScroll is a number then
            set the hscroll of pControl to tHScroll
         end if
         delete variable tAeChangeHScroll[pControl]
         send "aeChangeHScrollDone" to pControl
      end if
      send "aeExitFrame" &&"aeChangeHScroll" to pControl
   end repeat
   unlock screen
   put the milliseconds-tElapsed into tElapsed
   if the keys of tAeChangeHScroll is not empty then
      if tElapsed < 1000 / sAEFrameRate then
         if "aeChangingHScroll" is not in the pendingmessages then
            send "aeChangingHScroll" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeChangingHScroll" is not in the pendingmessages then
            send "aeChangingHScroll" to me in 5 millisecs
         end if
      end if
   end if
end aeChangingHScroll

on aeChangeHScrollDone
   
end aeChangeHScrollDone

local sAELockVScrollChanging,tAeChangeVScroll

on aeChangeVScroll
   local tControl,tVScroll,tDuration,tMethod,theValue
   if sAEFrameRate is empty then aeResetFrameRate
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tControl
   put item 2 of theValue into tVScroll
   put item 3 of theValue into tDuration
   put item 4 of theValue into tMethod
   if there is no tControl then return "error:"&&tControl&&"is not a valid control or group or stack"
   if word 1 of the long name of tControl is "card" then return "error: can not resize a card"
   if tVScroll is not an integer then return "error:"&&tVScroll&&"is not a valid VScroll"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tMethod&&"is not a valid easing method. Must be in,out or inOut"
   if not sAELockVScrollChanging then
      put tVScroll,tDuration,tMethod,the vscroll of tControl,the milliseconds into tAeChangeVScroll[the long id of tControl]
      if "aeChangingVScroll" is not in the pendingmessages then
         aeChangingVScroll
      end if
   else
      put tVScroll,tDuration,tMethod,the vscroll of tControl,"pending" into tAeChangeVScroll[the long id of tControl]
   end if
end aeChangeVScroll

on aeLockVScrolls
   put true into sAELockVScrollChanging
end aeLockVScrolls

on aeUnlockVScrolls
   local tNow
   put the milliseconds into tNow
   repeat for each line pControl in the keys of tAeChangeVScroll
      if item 5 of tAeChangeVScroll[pControl] <> "pending" then next repeat
      put tNow into item 5 of tAeChangeVScroll[pControl]
   end repeat
   put false into sAELockVScrollChanging
   if "aeChangingVScroll" is not in the pendingmessages then aeChangingVScroll
end aeUnlockVScrolls

local sTimeTakenToChangeVScrolls

/**

Summary: Animates the vscroll property of the target object
*/
command aeChangingVScroll
   lock screen
   local tTimeLost,tFramesLost,tElapsed
   local tVScroll,tDuration,tMethod,tStartVScroll,tDestVScroll,tStartTime
   if sAEFrameRate is not a number then aeResetFrameRate
   put the milliseconds into tElapsed
   put the milliseconds - sTimeTakenToChangeVScrolls into tTimeLost
   put tTimeLost - (1000/ sAEFrameRate) into tTimeLost
   put ((1000/sAeFramerate) + tTimeLost) into tFramesLost
   if tFramesLost<>0 then
      put 1000/tFramesLost into tFramesLost
   end if
   if tFramesLost>sAEFrameRate then put sAEFrameRate into tFramesLost
   put tFramesLost into sAERealFrameRate
   put the millisecs into sTimeTakenToChangeVScrolls
   --lock screen
   repeat for each line pControl in the keys of tAeChangeVScroll
      send "aeEnterFrame" &&"aeChangeVScroll" to pcontrol
      put item 1 of tAeChangeVScroll[pControl] into tDestVScroll
      put item 2 of tAeChangeVScroll[pControl] into tDuration
      put item 3 of tAeChangeVScroll[pControl] into tMethod
      put item 4 of tAeChangeVScroll[pControl] into tStartVScroll
      put item 5 of tAeChangeVScroll[pControl] into tStartTime
      if tStartTime="pending" then next repeat
      switch tMethod
         case "in"
            put aeEaseIn(tStartVScroll,tDestVScroll,tDuration,the milliseconds-tStartTime,3) into tVScroll
            break
         case "out"
            put aeEaseOut(tStartVScroll,tDestVScroll,tDuration,the milliseconds-tStartTime,3) into tVScroll
            break
         case "inOut"
            put aeEaseInOut(tStartVScroll,tDestVScroll,tDuration,the milliseconds-tStartTime,3) into tVScroll
            break
         case "bounce"
            put aeBounceEaseOut(tStartVScroll,tDestVScroll,tDuration,the milliseconds-tStartTime) into tVScroll
            break
         case "overshoot"
            put aeOverShootEaseOut(tStartVScroll,tDestVScroll,tDuration,the milliseconds-tStartTime) into tVScroll
            break
         default
            put aeEaseIn(tStartVScroll,tDestVScroll,tDuration,the milliseconds-tStartTime,1) into tVScroll
            break
      end switch
      put round(tVscroll) into tVscroll
      if the milliseconds-tStartTime<tDuration then
         set the vscroll of pControl to tVScroll
      else
         if tVScroll is a number then
            set the vscroll of pControl to tVScroll
         end if
         delete variable tAeChangeVScroll[pControl]
         send "aeChangeVScrollDone" to pControl
      end if
      send "aeExitFrame" &&"aeChangeVScroll" to pControl
   end repeat
   unlock screen
   put the milliseconds-tElapsed into tElapsed
   if the keys of tAeChangeVScroll is not empty then
      if tElapsed < 1000 / sAEFrameRate then
         if "aeChangingVScroll" is not in the pendingmessages then
            send "aeChangingVScroll" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeChangingVScroll" is not in the pendingmessages then
            send "aeChangingVScroll" to me in 5 millisecs
         end if
      end if
   end if
end aeChangingVScroll

on aeChangeVScrollDone
   
end aeChangeVScrollDone

local sAELockBackColorChanging,tAeChangeBackColor

on aeChangeBackColor
   local tControl,tBackColor,tDuration,tMethod,theValue,tCurrentBackColor
   if sAEFrameRate is empty then aeResetFrameRate
   /*
   -- Gott sei Dank bin ich den Scheiss mit der 5er Engine los geworden. Ein Grund mehr engine 4.x nicht mehr zu unterstützen.
   if "animationEngine" is not among the lines of the openstacks then
      -- Mein liebes zukünftiges Ich, bitte vergib mir!!!
      local tDefault
      put the defaultstack into tDefault
      open inv "animationEngine"
      set the defaultstack to tDefault
   end if
   */
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tControl
   if item 2 of theValue is a color and item 2 of theValue is not a number then
      -- we got no RGB values here
      set the backColor of the templateGraphic to (item 2 of theValue)
      set the backPixel of the templateGraphic to (the effective backPixel of the templateGraphic)
      put the backColor of the templateGraphic into tBackColor
      #set the backcolor of grc "testColor" of stack "animationEngine" to item 2 of theValue
      #set the backpixel of grc "testColor" of stack "animationEngine" to the effective backpixel of grc "testColor" of stack "animationEngine"
      #put the backcolor of grc "testColor" of stack "animationEngine" into tBackColor
      
      put item 3 of theValue into tDuration
      put item 4 of theValue into tMethod
   else
      put item 2 to 4 of theValue into tBackColor
      put item 5 of theValue into tDuration
      put item 6 of theValue into tMethod
   end if
   if there is no tControl then return "error:"&&tControl&&"is not a valid control or group or stack"
   if tBackColor is not a color then return "error:"&&tBackColor&&"is not a valid color"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tMethod&&"is not a valid easing method. Must be in,out or inOut"
   set the backColor of the templateGraphic to (the effective backColor of tControl)
      set the backPixel of the templateGraphic to (the effective backPixel of the templateGraphic)
      put the backColor of the templateGraphic into tCurrentBackColor
   #set the backcolor of grc "testColor" of stack "animationEngine" to the effective backcolor of tControl
   #set the backpixel of grc "testColor" of stack "animationEngine" to the effective backpixel of grc "testColor" of stack "animationEngine"
   #put the backcolor of grc "testColor" of stack "animationEngine" into tCurrentBackColor   
   if not sAELockBackColorChanging then
      put tBackColor,tDuration,tMethod,tCurrentBackColor,the milliseconds into tAeChangeBackColor[the long id of tControl]
      if "aeChangingBackColor" is not in the pendingmessages then
         aeChangingBackColor
      end if
   else
      put tBackColor,tDuration,tMethod,tCurrentBackColor,"pending" into tAeChangeBackColor[the long id of tControl]
   end if
end aeChangeBackColor

on aeLockBackColors
   put true into sAELockBackColorChanging
end aeLockBackColors

on aeUnlockBackColors
   local tNow
   put the milliseconds into tNow
   repeat for each line pControl in the keys of tAeChangeBackColor
      if item 9 of tAeChangeBackColor[pControl] <> "pending" then 
         next repeat
      end if
      put tNow into item 9 of tAeChangeBackColor[pControl]
   end repeat
   put false into sAELockBackColorChanging
   if "aeChangingBackColor" is not in the pendingmessages then aeChangingBackColor
end aeUnlockBackColors

local sTimeTakenToChangeBackColors

/**

Summary: Animates the backColor property of the target object
*/
command aeChangingBackColor
   
   lock screen
   local tTimeLost,tFramesLost,tElapsed
   local tBackColor,tDuration,tMethod,tStartBackColor,tDestBackColor,tStartTime
   if sAEFrameRate is not a number then aeResetFrameRate
   put the milliseconds into tElapsed
   put the milliseconds - sTimeTakenToChangeBackColors into tTimeLost
   put tTimeLost - (1000/ sAEFrameRate) into tTimeLost
   put ((1000/sAeFramerate) + tTimeLost) into tFramesLost
   if tFramesLost<>0 then
      put 1000/tFramesLost into tFramesLost
   end if
   if tFramesLost>sAEFrameRate then put sAEFrameRate into tFramesLost
   put tFramesLost into sAERealFrameRate
   put the millisecs into sTimeTakenToChangeBackColors
   --lock screen
   repeat for each line pControl in the keys of tAeChangeBackColor
      send "aeEnterFrame" && "aeChangeBackColor" to pcontrol
      put item 1 to 3 of tAeChangeBackColor[pControl] into tDestBackColor
      put item 4 of tAeChangeBackColor[pControl] into tDuration
      put item 5 of tAeChangeBackColor[pControl] into tMethod
      put item 6 to 8 of tAeChangeBackColor[pControl] into tStartBackColor
      put item 9 of tAeChangeBackColor[pControl] into tStartTime
      --      answer  tAeChangeBackColor[pControl]
      --      exit to top
      if tStartTime="pending" then next repeat
      switch tMethod
         case "in"
            put aeEaseIn(item 1 of tStartBackColor,item 1 of tDestBackColor,tDuration,the milliseconds-tStartTime,3) into item 1 of tBackColor
            put aeEaseIn(item 2 of tStartBackColor,item 2 of tDestBackColor,tDuration,the milliseconds-tStartTime,3) into item 2 of tBackColor
            put aeEaseIn(item 3 of tStartBackColor,item 3 of tDestBackColor,tDuration,the milliseconds-tStartTime,3) into item 3 of tBackColor
            break
         case "out"
            put aeEaseOut(item 1 of tStartBackColor,item 1 of tDestBackColor,tDuration,the milliseconds-tStartTime,3) into item 1 of tBackColor
            put aeEaseOut(item 2 of tStartBackColor,item 2 of tDestBackColor,tDuration,the milliseconds-tStartTime,3) into item 2 of tBackColor
            put aeEaseOut(item 3 of tStartBackColor,item 3 of tDestBackColor,tDuration,the milliseconds-tStartTime,3) into item 3 of tBackColor
            break
         case "inOut"
            put aeEaseInOut(item 1 of tStartBackColor,item 1 of tDestBackColor,tDuration,the milliseconds-tStartTime,3) into item 1 of tBackColor
            put aeEaseInOut(item 2 of tStartBackColor,item 2 of tDestBackColor,tDuration,the milliseconds-tStartTime,3) into item 2 of tBackColor
            put aeEaseInOut(item 3 of tStartBackColor,item 3 of tDestBackColor,tDuration,the milliseconds-tStartTime,3) into item 3 of tBackColor
            break
         case "bounce"
            put aeBounceEaseOut(item 1 of tStartBackColor,item 1 of tDestBackColor,tDuration,the milliseconds-tStartTime) into item 1 of tBackColor
            put aeBounceEaseOut(item 2 of tStartBackColor,item 2 of tDestBackColor,tDuration,the milliseconds-tStartTime) into item 2 of tBackColor
            put aeBounceEaseOut(item 3 of tStartBackColor,item 3 of tDestBackColor,tDuration,the milliseconds-tStartTime) into item 3 of tBackColor
            break
         case "overshoot"
            put aeOverShootEaseOut(item 1 of tStartBackColor,item 1 of tDestBackColor,tDuration,the milliseconds-tStartTime) into item 1 of tBackColor
            put aeOverShootEaseOut(item 2 of tStartBackColor,item 2 of tDestBackColor,tDuration,the milliseconds-tStartTime) into item 2 of tBackColor
            put aeOverShootEaseOut(item 3 of tStartBackColor,item 3 of tDestBackColor,tDuration,the milliseconds-tStartTime) into item 3 of tBackColor
            break
         default
            put aeEaseIn(item 1 of tStartBackColor,item 1 of tDestBackColor,tDuration,the milliseconds-tStartTime,1) into item 1 of tBackColor
            put aeEaseIn(item 2 of tStartBackColor,item 2 of tDestBackColor,tDuration,the milliseconds-tStartTime,1) into item 2 of tBackColor
            put aeEaseIn(item 3 of tStartBackColor,item 3 of tDestBackColor,tDuration,the milliseconds-tStartTime,1) into item 3 of tBackColor
            break
      end switch
      repeat with i=1 to 3
         put round(item i of tBackColor) into item i of tBackColor
         if item i of tBackColor > 255 then put 255 into item i of tBackColor
         if item i of tBackColor <0 then put 0 into item i of tBackColor
      end repeat
      if the milliseconds-tStartTime<tDuration then
         set the backcolor of pControl to tBackColor
      else
         if tBackColor is a color then
            set the backcolor of pControl to tBackColor
         end if
         delete variable tAeChangeBackColor[pControl]
         send "aeChangeBackColorDone" to pControl
      end if
      send "aeExitFrame" && "aeChangeBackColor" to pControl
   end repeat
   unlock screen
   put the milliseconds-tElapsed into tElapsed
   if the keys of tAeChangeBackColor is not empty then
      if tElapsed < 1000 / sAEFrameRate then
         if "aeChangingBackColor" is not in the pendingmessages then
            send "aeChangingBackColor" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeChangingBackColor" is not in the pendingmessages then
            send "aeChangingBackColor" to me in 5 millisecs
         end if
      end if
   end if
end aeChangingBackColor

on aeChangeBackColorDone
   
end aeChangeBackColorDone

local sAELockforeColorChanging,tAeChangeforeColor

on aeChangeforeColor
   local tControl,tforeColor,tDuration,tMethod,theValue,tCurrentforeColor
   if sAEFrameRate is empty then aeResetFrameRate
   --   if "animationEngine" is not among the lines of the openstacks then
   --      local tDefault
   --      put the defaultstack into tDefault
   --      open inv "animationEngine"
   --      set the defaultstack to tDefault
   --  -- WAAAAAAAAAAAAAAAAAAAAHHHHHHHHHHHHHHHH... Warum nur. Warum....
   --   end if
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tControl
   if item 2 of theValue is a color and item 2 of theValue is not a number then
      -- we got no RGB values here
      set the backColor of the templateGraphic to (item 2 of theValue)
      set the backPixel of the templateGraphic to (the effective backPixel of the templateGraphic)
      put the backColor of the templateGraphic into tForeColor
      #set the backcolor of grc "testColor" of stack "animationEngine" to item 2 of theValue
      #set the backpixel of grc "testColor" of stack "animationEngine" to the effective backpixel of grc "testColor" of stack "animationEngine"
      #put the backcolor of grc "testColor" of stack "animationEngine" into tforeColor
      put item 3 of theValue into tDuration
      put item 4 of theValue into tMethod
   else
      put item 2 to 4 of theValue into tforeColor
      put item 5 of theValue into tDuration
      put item 6 of theValue into tMethod
   end if
   if there is no tControl then return "error:"&&tControl&&"is not a valid control or group or stack"
   if tforeColor is not a color then return "error:"&&tforeColor&&"is not a valid color"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tMethod&&"is not a valid easing method. Must be in,out or inOut"
   set the backColor of the templateGraphic to (the effective foreColor of tControl)
      set the backPixel of the templateGraphic to (the effective backPixel of the templateGraphic)
      put the backColor of the templateGraphic into tCurrentForeColor
   #set the backcolor of grc "testColor" of stack "animationEngine" to the effective forecolor of tControl
   #set the backpixel of grc "testColor" of stack "animationEngine" to the effective backpixel of grc "testColor" of stack "animationEngine"
   #put the backcolor of grc "testColor" of stack "animationEngine" into tCurrentforeColor   
   if not sAELockforeColorChanging then
      put tforeColor,tDuration,tMethod,tCurrentforeColor,the milliseconds into tAeChangeforeColor[the long id of tControl]
      if "aeChangingforeColor" is not in the pendingmessages then
         aeChangingforeColor
      end if
   else
      put tforeColor,tDuration,tMethod,tCurrentforeColor,"pending" into tAeChangeforeColor[the long id of tControl]
   end if
end aeChangeforeColor

on aeLockforeColors
   put true into sAELockforeColorChanging
end aeLockforeColors

on aeUnlockforeColors
   local tNow
   put the milliseconds into tNow
   repeat for each line pControl in the keys of tAeChangeforeColor
      if item 5 of tAeChangeforeColor[pControl] <> "pending" then next repeat
      put tNow into item 5 of tAeChangeforeColor[pControl]
   end repeat
   put false into sAELockforeColorChanging
   if "aeChangingforeColor" is not in the pendingmessages then aeChangingforeColor
end aeUnlockforeColors

local sTimeTakenToChangeforeColors

/**

Summary: Animates the foreColor property of the target object
*/
command aeChangingforeColor
   
   lock screen
   local tTimeLost,tFramesLost,tElapsed
   local tforeColor,tDuration,tMethod,tStartforeColor,tDestforeColor,tStartTime
   if sAEFrameRate is not a number then aeResetFrameRate
   put the milliseconds into tElapsed
   put the milliseconds - sTimeTakenToChangeforeColors into tTimeLost
   put tTimeLost - (1000/ sAEFrameRate) into tTimeLost
   put ((1000/sAeFramerate) + tTimeLost) into tFramesLost
   if tFramesLost<>0 then
      put 1000/tFramesLost into tFramesLost
   end if
   if tFramesLost>sAEFrameRate then put sAEFrameRate into tFramesLost
   put tFramesLost into sAERealFrameRate
   put the millisecs into sTimeTakenToChangeforeColors
   --lock screen
   repeat for each line pControl in the keys of tAeChangeforeColor
      send "aeEnterFrame" && "aeChangeForeColor" to pcontrol
      put item 1 to 3 of tAeChangeforeColor[pControl] into tDestforeColor
      put item 4 of tAeChangeforeColor[pControl] into tDuration
      put item 5 of tAeChangeforeColor[pControl] into tMethod
      put item 6 to 8 of tAeChangeforeColor[pControl] into tStartforeColor
      put item 9 of tAeChangeforeColor[pControl] into tStartTime
      --      answer  tAeChangeforeColor[pControl]
      --      exit to top
      if tStartTime="pending" then next repeat
      switch tMethod
         case "in"
            put aeEaseIn(item 1 of tStartforeColor,item 1 of tDestforeColor,tDuration,the milliseconds-tStartTime,3) into item 1 of tforeColor
            put aeEaseIn(item 2 of tStartforeColor,item 2 of tDestforeColor,tDuration,the milliseconds-tStartTime,3) into item 2 of tforeColor
            put aeEaseIn(item 3 of tStartforeColor,item 3 of tDestforeColor,tDuration,the milliseconds-tStartTime,3) into item 3 of tforeColor
            break
         case "out"
            put aeEaseOut(item 1 of tStartforeColor,item 1 of tDestforeColor,tDuration,the milliseconds-tStartTime,3) into item 1 of tforeColor
            put aeEaseOut(item 2 of tStartforeColor,item 2 of tDestforeColor,tDuration,the milliseconds-tStartTime,3) into item 2 of tforeColor
            put aeEaseOut(item 3 of tStartforeColor,item 3 of tDestforeColor,tDuration,the milliseconds-tStartTime,3) into item 3 of tforeColor
            break
         case "inOut"
            put aeEaseInOut(item 1 of tStartforeColor,item 1 of tDestforeColor,tDuration,the milliseconds-tStartTime,3) into item 1 of tforeColor
            put aeEaseInOut(item 2 of tStartforeColor,item 2 of tDestforeColor,tDuration,the milliseconds-tStartTime,3) into item 2 of tforeColor
            put aeEaseInOut(item 3 of tStartforeColor,item 3 of tDestforeColor,tDuration,the milliseconds-tStartTime,3) into item 3 of tforeColor
            break
         case "bounce"
            put aeBounceEaseOut(item 1 of tStartforeColor,item 1 of tDestforeColor,tDuration,the milliseconds-tStartTime) into item 1 of tforeColor
            put aeBounceEaseOut(item 2 of tStartforeColor,item 2 of tDestforeColor,tDuration,the milliseconds-tStartTime) into item 2 of tforeColor
            put aeBounceEaseOut(item 3 of tStartforeColor,item 3 of tDestforeColor,tDuration,the milliseconds-tStartTime) into item 3 of tforeColor
            break
         case "overshoot"
            put aeOverShootEaseOut(item 1 of tStartforeColor,item 1 of tDestforeColor,tDuration,the milliseconds-tStartTime) into item 1 of tforeColor
            put aeOverShootEaseOut(item 2 of tStartforeColor,item 2 of tDestforeColor,tDuration,the milliseconds-tStartTime) into item 2 of tforeColor
            put aeOverShootEaseOut(item 3 of tStartforeColor,item 3 of tDestforeColor,tDuration,the milliseconds-tStartTime) into item 3 of tforeColor
            break
         default
            put aeEaseIn(item 1 of tStartforeColor,item 1 of tDestforeColor,tDuration,the milliseconds-tStartTime,1) into item 1 of tforeColor
            put aeEaseIn(item 2 of tStartforeColor,item 2 of tDestforeColor,tDuration,the milliseconds-tStartTime,1) into item 2 of tforeColor
            put aeEaseIn(item 3 of tStartforeColor,item 3 of tDestforeColor,tDuration,the milliseconds-tStartTime,1) into item 3 of tforeColor
            break
      end switch
      repeat with i=1 to 3
         put round(item i of tforeColor) into item i of tforeColor
         if item i of tforeColor > 255 then put 255 into item i of tforeColor
         if item i of tforeColor <0 then put 0 into item i of tforeColor
      end repeat
      if the milliseconds-tStartTime<tDuration then
         set the forecolor of pControl to tforeColor
      else
         if tforeColor is a color then
            set the forecolor of pControl to tforeColor
         end if
         delete variable tAeChangeforeColor[pControl]
         send "aeChangeforeColorDone" to pControl
      end if
      send "aeExitFrame" && "aeChangeForeColor" to pControl
   end repeat
   unlock screen
   put the milliseconds-tElapsed into tElapsed
   if the keys of tAeChangeforeColor is not empty then
      if tElapsed < 1000 / sAEFrameRate then
         if "aeChangingforeColor" is not in the pendingmessages then
            send "aeChangingforeColor" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeChangingforeColor" is not in the pendingmessages then
            send "aeChangingforeColor" to me in 5 millisecs
         end if
      end if
   end if
end aeChangingforeColor

on aeChangeforeColorDone
   
end aeChangeforeColorDone

local sAELockThumbPositionChanging,tAeChangeThumbPosition

on aeChangeThumbPosition
   local tControl,tThumbPosition,tDuration,tMethod,theValue,tType
   if sAEFrameRate is empty then aeResetFrameRate
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tControl
   put item 2 of theValue into tThumbPosition
   put item 3 of theValue into tDuration
   put item 4 of theValue into tMethod
   if there is no tControl then return "error:"&&tControl&&"does not exist"
   put word 1 of the long name of tControl into tType
   if word 1 of the long name of tControl is not among the items of "scrollbar" then return "error: can not set the thumbPosition of a"&&tType
   if tThumbPosition is not an integer then return "error:"&&tThumbPosition&&"is not a valid ThumbPosition"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tMethod&&"is not a valid easing method. Must be in,out or inOut"
   if not sAELockThumbPositionChanging then
      put tThumbPosition,tDuration,tMethod,the thumbposition of tControl,the milliseconds into tAeChangeThumbPosition[the long id of tControl]
      if "aeChangingThumbPosition" is not in the pendingmessages then
         aeChangingThumbPosition
      end if
   else
      put tThumbPosition,tDuration,tMethod,the thumbposition of tControl,"pending" into tAeChangeThumbPosition[the long id of tControl]
   end if
end aeChangeThumbPosition

on aeLockThumbPositions
   put true into sAELockThumbPositionChanging
end aeLockThumbPositions

on aeUnlockThumbPositions
   local tNow
   put the milliseconds into tNow
   repeat for each line pControl in the keys of tAeChangeThumbPosition
      if item 5 of tAeChangeThumbPosition[pControl] <> "pending" then next repeat
      put tNow into item 5 of tAeChangeThumbPosition[pControl]
   end repeat
   put false into sAELockThumbPositionChanging
   if "aeChangingThumbPosition" is not in the pendingmessages then aeChangingThumbPosition
end aeUnlockThumbPositions

local sTimeTakenToChangeThumbPositions

/**

Summary: Animates the thumbPosition property of the target object
*/
command aeChangingThumbPosition
   lock screen
   local tTimeLost,tFramesLost,tElapsed
   local tThumbPosition,tDuration,tMethod,tStartThumbPosition,tDestThumbPosition,tStartTime
   if sAEFrameRate is not a number then aeResetFrameRate
   put the milliseconds into tElapsed
   put the milliseconds - sTimeTakenToChangeThumbPositions into tTimeLost
   put tTimeLost - (1000/ sAEFrameRate) into tTimeLost
   put ((1000/sAeFramerate) + tTimeLost) into tFramesLost
   if tFramesLost<>0 then
      put 1000/tFramesLost into tFramesLost
   end if
   if tFramesLost>sAEFrameRate then put sAEFrameRate into tFramesLost
   put tFramesLost into sAERealFrameRate
   put the millisecs into sTimeTakenToChangeThumbPositions
   --lock screen
   repeat for each line pControl in the keys of tAeChangeThumbPosition
      send "aeEnterFrame" && "aeChangeThumbPosition" to pcontrol
      put item 1 of tAeChangeThumbPosition[pControl] into tDestThumbPosition
      put item 2 of tAeChangeThumbPosition[pControl] into tDuration
      put item 3 of tAeChangeThumbPosition[pControl] into tMethod
      put item 4 of tAeChangeThumbPosition[pControl] into tStartThumbPosition
      put item 5 of tAeChangeThumbPosition[pControl] into tStartTime
      if tStartTime="pending" then next repeat
      switch tMethod
         case "in"
            put aeEaseIn(tStartThumbPosition,tDestThumbPosition,tDuration,the milliseconds-tStartTime,3) into tThumbPosition
            break
         case "out"
            put aeEaseOut(tStartThumbPosition,tDestThumbPosition,tDuration,the milliseconds-tStartTime,3) into tThumbPosition
            break
         case "inOut"
            put aeEaseInOut(tStartThumbPosition,tDestThumbPosition,tDuration,the milliseconds-tStartTime,3) into tThumbPosition
            break
         case "bounce"
            put aeBounceEaseOut(tStartThumbPosition,tDestThumbPosition,tDuration,the milliseconds-tStartTime) into tThumbPosition
            break
         case "overshoot"
            put aeOverShootEaseOut(tStartThumbPosition,tDestThumbPosition,tDuration,the milliseconds-tStartTime) into tThumbPosition
            break
         default
            put aeEaseIn(tStartThumbPosition,tDestThumbPosition,tDuration,the milliseconds-tStartTime,1) into tThumbPosition
            break
      end switch
      put round(tThumbPosition) into tThumbPosition
      if the milliseconds-tStartTime<tDuration then         
         set the thumbposition of pControl to tThumbPosition
      else
         if tThumbPosition is a number then
            set the thumbposition of pControl to tThumbPosition
         end if
         delete variable tAeChangeThumbPosition[pControl]
         send "aeChangeThumbPositionDone" to pControl
      end if
      send "aeExitFrame" && "aeChangeThumbPosition" to pControl
   end repeat
   unlock screen
   put the milliseconds-tElapsed into tElapsed
   if the keys of tAeChangeThumbPosition is not empty then
      if tElapsed < 1000 / sAEFrameRate then
         if "aeChangingThumbPosition" is not in the pendingmessages then
            send "aeChangingThumbPosition" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeChangingThumbPosition" is not in the pendingmessages then
            send "aeChangingThumbPosition" to me in 5 millisecs
         end if
      end if
   end if
end aeChangingThumbPosition

on aeChangeThumbPositionDone
   
end aeChangeThumbPositionDone

--------

local sAELockBackColorOfChunkChanging,tAeChangeBackColorOfChunk

on aeChangeBackColorOfChunk
   local tControl,tBackColorOfChunk,tDuration,tMethod,theValue,tCurrentBackColorOfChunk,tChunk
   
   if sAEFrameRate is empty then aeResetFrameRate
   --   if "animationEngine" is not among the lines of the openstacks then
   -- -- Und noch mal... Shit.
   --      local tDefault
   --      put the defaultstack into tDefault
   --      open inv "animationEngine"
   --      set the defaultstack to tDefault
   --   end if
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   
   put item 1 of theValue into tControl
   put item 2 of theValue into tChunk
   local tTest,tTest2
   put tChunk && "of" &&tControl into tTest
   try
      get tTest
   catch theErr
      return "aeError: Could not evaluate "&& tchunk&&"of"&&tControl
   end try
   put tTest into tTest2
   if word 1 of tTest2="char" then
      repeat with i=1 to the number of words of tTest2
         if word i of tTest2= "of" then exit repeat
      end repeat
      put "char 1 of" into word 1 to i of tTest2
   else
      put "char 1 of"&&tTest into tTest2
   end if
   if item 3 of theValue is a color and item 2 of theValue is not a number then
      -- we got no RGB values here
      set the backColor of the templateGraphic to (item 3 of theValue)
      set the backPixel of the templateGraphic to (the effective backPixel of the templateGraphic)
      put the backColor of the templateGraphic into tBackColorOfChunk
      #set the backcolor of grc "testColor" of stack "animationEngine" to item 3 of theValue
      #set the backpixel of grc "testColor" of stack "animationEngine" to the effective backpixel of grc "testColor" of stack "animationEngine"
      #put the backcolor of grc "testColor" of stack "animationEngine" into tBackColorOfChunk
      
      put item 4 of theValue into tDuration
      put item 5 of theValue into tMethod
   else
      put item 3 to 5 of theValue into tBackColorOfChunk
      put item 6 of theValue into tDuration
      put item 7 of theValue into tMethod
   end if
   if there is no tControl then return "error:"&&tControl&&"is not a valid control or group or stack"
   if tBackColorOfChunk is not a color then return "error:"&&tBackColorOfChunk&&"is not a valid color"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tMethod&&"is not a valid easing method. Must be in,out or inOut"
   set the backColor of the templateGraphic to (the effective backColor of tTest2)
      set the backPixel of the templateGraphic to (the effective backPixel of the templateGraphic)
      put the backColor of the templateGraphic into tCurrentBackColorOfChunk
   #set the backcolor of grc "testColor" of stack "animationEngine" to the effective backcolor of tTest2
   #set the backpixel of grc "testColor" of stack "animationEngine" to the effective backpixel of grc "testColor" of stack "animationEngine"
   #put the backcolor of grc "testColor" of stack "animationEngine" into tCurrentBackColorOfChunk   
   local tKey
   put (tControl&","&tChunk) into tKey
   if not sAELockBackColorOfChunkChanging then
      put tBackColorOfChunk,tChunk,tDuration,tMethod,tCurrentBackColorOfChunk,the milliseconds into tAeChangeBackColorOfChunk[tKey]
      if "aeChangingBackColorOfChunk" is not in the pendingmessages then
         aeChangingBackColorOfChunk
      end if
   else
      put tBackColorOfChunk,tChunk,tDuration,tMethod,tCurrentBackColorOfChunk,"pending" into tAeChangeBackColorOfChunk[tKey]
   end if
   
end aeChangeBackColorOfChunk

on aeLockBackColorOfChunks
   put true into sAELockBackColorOfChunkChanging
end aeLockBackColorOfChunks

on aeUnlockBackColorOfChunks
   local tNow
   put the milliseconds into tNow
   repeat for each line pControl in the keys of tAeChangeBackColorOfChunk
      if item 6 of tAeChangeBackColorOfChunk[pControl] <> "pending" then next repeat
      put tNow into item 6 of tAeChangeBackColorOfChunk[pControl]
   end repeat
   put false into sAELockBackColorOfChunkChanging
   if "aeChangingBackColorOfChunk" is not in the pendingmessages then aeChangingBackColorOfChunk
end aeUnlockBackColorOfChunks

local sTimeTakenToChangeBackColorOfChunks

/**

Summary: Animates the backColor of a text chunk in the target object
*/
command aeChangingBackColorOfChunk
   lock screen
   local tTimeLost,tFramesLost,tElapsed
   local tBackColorOfChunk,tDuration,tMethod,tStartBackColorOfChunk,tDestBackColorOfChunk,tStartTime,tChunk
   if sAEFrameRate is not a number then aeResetFrameRate
   put the milliseconds into tElapsed
   put the milliseconds - sTimeTakenToChangeBackColorOfChunks into tTimeLost
   put tTimeLost - (1000/ sAEFrameRate) into tTimeLost
   put ((1000/sAeFramerate) + tTimeLost) into tFramesLost
   if tFramesLost<>0 then
      put 1000/tFramesLost into tFramesLost
   end if
   if tFramesLost>sAEFrameRate then put sAEFrameRate into tFramesLost
   put tFramesLost into sAERealFrameRate
   put the millisecs into sTimeTakenToChangeBackColorOfChunks
   local theControl
   repeat for each line pControl in the keys of tAeChangeBackColorOfChunk
      put item 1 of pControl into theControl
      send "aeEnterFrame" && "aeChangeBackColorOfChunk" to theControl
      put item 1 to 3 of tAeChangeBackColorOfChunk[pControl] into tDestBackColorOfChunk
      put item 4 of  tAeChangeBackColorOfChunk[pControl] into tChunk
      put item 5 of tAeChangeBackColorOfChunk[pControl] into tDuration
      put item 6 of tAeChangeBackColorOfChunk[pControl] into tMethod
      put item 7 to 9 of tAeChangeBackColorOfChunk[pControl] into tStartBackColorOfChunk
      put item 10 of tAeChangeBackColorOfChunk[pControl] into tStartTime
      if tStartTime="pending" then next repeat
      switch tMethod
         case "in"
            put aeEaseIn(item 1 of tStartBackColorOfChunk,item 1 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 1 of tBackColorOfChunk
            put aeEaseIn(item 2 of tStartBackColorOfChunk,item 2 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 2 of tBackColorOfChunk
            put aeEaseIn(item 3 of tStartBackColorOfChunk,item 3 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 3 of tBackColorOfChunk
            break
         case "out"
            put aeEaseOut(item 1 of tStartBackColorOfChunk,item 1 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 1 of tBackColorOfChunk
            put aeEaseOut(item 2 of tStartBackColorOfChunk,item 2 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 2 of tBackColorOfChunk
            put aeEaseOut(item 3 of tStartBackColorOfChunk,item 3 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 3 of tBackColorOfChunk
            break
         case "inOut"
            put aeEaseInOut(item 1 of tStartBackColorOfChunk,item 1 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 1 of tBackColorOfChunk
            put aeEaseInOut(item 2 of tStartBackColorOfChunk,item 2 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 2 of tBackColorOfChunk
            put aeEaseInOut(item 3 of tStartBackColorOfChunk,item 3 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 3 of tBackColorOfChunk
            break
         case "bounce"
            put aeBounceEaseOut(item 1 of tStartBackColorOfChunk,item 1 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime) into item 1 of tBackColorOfChunk
            put aeBounceEaseOut(item 2 of tStartBackColorOfChunk,item 2 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime) into item 2 of tBackColorOfChunk
            put aeBounceEaseOut(item 3 of tStartBackColorOfChunk,item 3 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime) into item 3 of tBackColorOfChunk
            break
         case "overshoot"
            put aeOverShootEaseOut(item 1 of tStartBackColorOfChunk,item 1 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime) into item 1 of tBackColorOfChunk
            put aeOverShootEaseOut(item 2 of tStartBackColorOfChunk,item 2 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime) into item 2 of tBackColorOfChunk
            put aeOverShootEaseOut(item 3 of tStartBackColorOfChunk,item 3 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime) into item 3 of tBackColorOfChunk
            break
         default
            put aeEaseIn(item 1 of tStartBackColorOfChunk,item 1 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime,1) into item 1 of tBackColorOfChunk
            put aeEaseIn(item 2 of tStartBackColorOfChunk,item 2 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime,1) into item 2 of tBackColorOfChunk
            put aeEaseIn(item 3 of tStartBackColorOfChunk,item 3 of tDestBackColorOfChunk,tDuration,the milliseconds-tStartTime,1) into item 3 of tBackColorOfChunk
            break
      end switch
      repeat with i=1 to 3
         put round(item i of tBackColorOfChunk) into item i of tBackColorOfChunk
         if item i of tBackColorOfChunk > 255 then put 255 into item i of tBackColorOfChunk
         if item i of tBackColorOfChunk <0 then put 0 into item i of tBackColorOfChunk
      end repeat
      local assembleChunk
      put tChunk &" of " & item 1 of pControl into assembleChunk
      if the milliseconds-tStartTime<tDuration then
         do "set the BackColor of"&& assembleChunk&& "to" && tBackColorOfChunk
      else
         if tBackColorOfChunk is a color then
            do "set the BackColor of"&& assembleChunk&& "to" && tBackColorOfChunk
         end if
         delete variable tAeChangeBackColorOfChunk[pControl]
         send "aeChangeBackColorOfChunkDone" to theControl
      end if
      send "aeExitFrame" && "aeChangeBackColorOfChunk" to theControl
   end repeat 
   unlock screen
   put the milliseconds-tElapsed into tElapsed
   if the keys of tAeChangeBackColorOfChunk is not empty then
      if tElapsed < 1000 / sAEFrameRate then
         if "aeChangingBackColorOfChunk" is not in the pendingmessages then
            send "aeChangingBackColorOfChunk" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeChangingBackColorOfChunk" is not in the pendingmessages then
            send "aeChangingBackColorOfChunk" to me in 5 millisecs
         end if
      end if
   end if
end aeChangingBackColorOfChunk

on aeChangeBackColorOfChunkDone
   
end aeChangeBackColorOfChunkDone

------
local sAELockforecolorOfChunkChanging,tAeChangeforecolorOfChunk

on aeChangeforecolorOfChunk
   local tControl,tforecolorOfChunk,tDuration,tMethod,theValue,tCurrentforecolorOfChunk,tChunk
   
   if sAEFrameRate is empty then aeResetFrameRate
   --   if "animationEngine" is not among the lines of the openstacks then
   -- -- Das selbe Spiel. *Spuck*
   --      local tDefault
   --      put the defaultstack into tDefault
   --      open inv "animationEngine"
   --      set the defaultstack to tDefault
   --   end if
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   
   put item 1 of theValue into tControl
   put item 2 of theValue into tChunk
   local tTest,tTest2
   put tChunk && "of" &&tControl into tTest
   try
      get tTest
   catch theErr
      return "aeError: Could not evaluate "&& tchunk&&"of"&&tControl
   end try
   put tTest into tTest2
   if word 1 of tTest2="char" then
      repeat with i=1 to the number of words of tTest2
         if word i of tTest2= "of" then exit repeat
      end repeat
      put "char 1 of" into word 1 to i of tTest2
   else
      put "char 1 of"&&tTest into tTest2
   end if
   if item 3 of theValue is a color and item 2 of theValue is not a number then
      -- we got no RGB values here
      set the backColor of the templateGraphic to (item 3 of theValue)
      set the backPixel of the templateGraphic to (the effective backPixel of the templateGraphic)
      put the backColor of the templateGraphic into tForeColorOfChunk
      #set the backcolor of grc "testColor" of stack "animationEngine" to item 3 of theValue
      #set the backpixel of grc "testColor" of stack "animationEngine" to the effective backpixel of grc "testColor" of stack "animationEngine"
      #put the backcolor of grc "testColor" of stack "animationEngine" into tforecolorOfChunk
      
      put item 4 of theValue into tDuration
      put item 5 of theValue into tMethod
   else
      put item 3 to 5 of theValue into tforecolorOfChunk
      put item 6 of theValue into tDuration
      put item 7 of theValue into tMethod
   end if
   if there is no tControl then return "error:"&&tControl&&"is not a valid control or group or stack"
   if tforecolorOfChunk is not a color then return "error:"&&tforecolorOfChunk&&"is not a valid color"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tMethod&&"is not a valid easing method. Must be in,out or inOut"
   set the backColor of the templateGraphic to (the effective foreColor of tTest2)
      set the backPixel of the templateGraphic to (the effective backPixel of the templateGraphic)
      put the backColor of the templateGraphic into tCurrentForeColorOfChunk
   #set the backcolor of grc "testColor" of stack "animationEngine" to the effective forecolor of tTest2
   #set the backpixel of grc "testColor" of stack "animationEngine" to the effective backpixel of grc "testColor" of stack "animationEngine"
   #put the backcolor of grc "testColor" of stack "animationEngine" into tCurrentforecolorOfChunk  
   local tKey
   put (tControl&","&tChunk) into tKey
   if not sAELockforecolorOfChunkChanging then
      put tforecolorOfChunk,tChunk,tDuration,tMethod,tCurrentforecolorOfChunk,the milliseconds into tAeChangeforecolorOfChunk[tKey]
      if "aeChangingforecolorOfChunk" is not in the pendingmessages then
         aeChangingforecolorOfChunk
      end if
   else
      put tforecolorOfChunk,tChunk,tDuration,tMethod,tCurrentforecolorOfChunk,"pending" into tAeChangeforecolorOfChunk[tKey]
   end if
   
end aeChangeforecolorOfChunk

on aeLockforecolorOfChunks
   put true into sAELockforecolorOfChunkChanging
end aeLockforecolorOfChunks

on aeUnlockforecolorOfChunks
   local tNow
   put the milliseconds into tNow
   repeat for each line pControl in the keys of tAeChangeforecolorOfChunk
      if item 6 of tAeChangeforecolorOfChunk[pControl] <> "pending" then next repeat
      put tNow into item 6 of tAeChangeforecolorOfChunk[pControl]
   end repeat
   put false into sAELockforecolorOfChunkChanging
   if "aeChangingforecolorOfChunk" is not in the pendingmessages then aeChangingforecolorOfChunk
end aeUnlockforecolorOfChunks

local sTimeTakenToChangeforecolorOfChunks

/**

Summary: Animates the foreColor of a text chunk in the target object
*/
command aeChangingforecolorOfChunk
   lock screen
   local tTimeLost,tFramesLost,tElapsed
   local tforecolorOfChunk,tDuration,tMethod,tStartforecolorOfChunk,tDestforecolorOfChunk,tStartTime,tChunk
   if sAEFrameRate is not a number then aeResetFrameRate
   put the milliseconds into tElapsed
   put the milliseconds - sTimeTakenToChangeforecolorOfChunks into tTimeLost
   put tTimeLost - (1000/ sAEFrameRate) into tTimeLost
   put ((1000/sAeFramerate) + tTimeLost) into tFramesLost
   if tFramesLost<>0 then
      put 1000/tFramesLost into tFramesLost
   end if
   if tFramesLost>sAEFrameRate then put sAEFrameRate into tFramesLost
   put tFramesLost into sAERealFrameRate
   put the millisecs into sTimeTakenToChangeforecolorOfChunks
   local theControl
   repeat for each line pControl in the keys of tAeChangeforecolorOfChunk
      put item 1 of pControl into theControl
      send "aeEnterFrame" && "aeChangeForeColorOfChunk" to theControl
      put item 1 to 3 of tAeChangeforecolorOfChunk[pControl] into tDestforecolorOfChunk
      put item 4 of  tAeChangeforecolorOfChunk[pControl] into tChunk
      put item 5 of tAeChangeforecolorOfChunk[pControl] into tDuration
      put item 6 of tAeChangeforecolorOfChunk[pControl] into tMethod
      put item 7 to 9 of tAeChangeforecolorOfChunk[pControl] into tStartforecolorOfChunk
      put item 10 of tAeChangeforecolorOfChunk[pControl] into tStartTime
      if tStartTime="pending" then next repeat
      switch tMethod
         case "in"
            put aeEaseIn(item 1 of tStartforecolorOfChunk,item 1 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 1 of tforecolorOfChunk
            put aeEaseIn(item 2 of tStartforecolorOfChunk,item 2 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 2 of tforecolorOfChunk
            put aeEaseIn(item 3 of tStartforecolorOfChunk,item 3 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 3 of tforecolorOfChunk
            break
         case "out"
            put aeEaseOut(item 1 of tStartforecolorOfChunk,item 1 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 1 of tforecolorOfChunk
            put aeEaseOut(item 2 of tStartforecolorOfChunk,item 2 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 2 of tforecolorOfChunk
            put aeEaseOut(item 3 of tStartforecolorOfChunk,item 3 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 3 of tforecolorOfChunk
            break
         case "inOut"
            put aeEaseInOut(item 1 of tStartforecolorOfChunk,item 1 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 1 of tforecolorOfChunk
            put aeEaseInOut(item 2 of tStartforecolorOfChunk,item 2 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 2 of tforecolorOfChunk
            put aeEaseInOut(item 3 of tStartforecolorOfChunk,item 3 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime,3) into item 3 of tforecolorOfChunk
            break
         case "bounce"
            put aeBounceEaseOut(item 1 of tStartforecolorOfChunk,item 1 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime) into item 1 of tforecolorOfChunk
            put aeBounceEaseOut(item 2 of tStartforecolorOfChunk,item 2 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime) into item 2 of tforecolorOfChunk
            put aeBounceEaseOut(item 3 of tStartforecolorOfChunk,item 3 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime) into item 3 of tforecolorOfChunk
            break
         case "overshoot"
            put aeOverShootEaseOut(item 1 of tStartforecolorOfChunk,item 1 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime) into item 1 of tforecolorOfChunk
            put aeOverShootEaseOut(item 2 of tStartforecolorOfChunk,item 2 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime) into item 2 of tforecolorOfChunk
            put aeOverShootEaseOut(item 3 of tStartforecolorOfChunk,item 3 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime) into item 3 of tforecolorOfChunk
            break
         default
            put aeEaseIn(item 1 of tStartforecolorOfChunk,item 1 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime,1) into item 1 of tforecolorOfChunk
            put aeEaseIn(item 2 of tStartforecolorOfChunk,item 2 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime,1) into item 2 of tforecolorOfChunk
            put aeEaseIn(item 3 of tStartforecolorOfChunk,item 3 of tDestforecolorOfChunk,tDuration,the milliseconds-tStartTime,1) into item 3 of tforecolorOfChunk
            break
      end switch
      
      repeat with i=1 to 3
         put round(item i of tforecolorOfChunk) into item i of tforecolorOfChunk
         if item i of tforecolorOfChunk > 255 then put 255 into item i of tforecolorOfChunk
         if item i of tforecolorOfChunk <0 then put 0 into item i of tforecolorOfChunk
      end repeat
      local assembleChunk
      put tChunk &" of " & item 1 of pControl into assembleChunk
      if the milliseconds-tStartTime<tDuration then
         do "set the forecolor of"&& assembleChunk&& "to" && tforecolorOfChunk
      else
         if tforecolorOfChunk is a color then
            do "set the forecolor of"&& assembleChunk&& "to" && tforecolorOfChunk
         end if
         delete variable tAeChangeforecolorOfChunk[pControl]
         send "aeChangeforecolorOfChunkDone" to theControl
      end if
      send "aeExitFrame" && "aeChangeForeColorOfChunk" to theControl
   end repeat
   unlock screen
   put the milliseconds-tElapsed into tElapsed
   if the keys of tAeChangeforecolorOfChunk is not empty then
      if tElapsed < 1000 / sAEFrameRate then
         if "aeChangingforecolorOfChunk" is not in the pendingmessages then
            send "aeChangingforecolorOfChunk" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeChangingforecolorOfChunk" is not in the pendingmessages then
            send "aeChangingforecolorOfChunk" to me in 5 millisecs
         end if
      end if
   end if
end aeChangingforecolorOfChunk

on aeChangeforecolorOfChunkDone
   
end aeChangeforecolorOfChunkDone

-- stop stuff

on aeStopChangingForeColorOfChunk tControl,tChunk
   local tKey
   put (tControl&","&tChunk) into tKey
   if tControl="all" then
      delete variable tAeChangeforecolorOfChunk
   else
      delete variable tAeChangeforecolorOfChunk[tKey]
   end if
end aeStopChangingForeColorOfChunk

on aeStopChangingBackColorOfChunk tControl,tChunk
   local tKey
   put (tControl&","&tChunk) into tKey
   if tControl="all" then
      delete variable tAeChangeBackcolorOfChunk
   else
      delete variable tAeChangeBackcolorOfChunk[tKey]
   end if
end aeStopChangingBackColorOfChunk

on aeStopChangingVscroll tControl
   if tControl="all" then
      delete variable tAEChangeVscroll
   else
      delete variable tAEChangeVscroll[the long id of tControl]
   end if
end aeStopChangingVScroll

on aeStopChangingHscroll tControl
   if tControl="all" then
      delete variable tAEChangeHscroll
   else
      delete variable tAEChangeHscroll[the long id of tControl]
   end if
end aeStopChangingHScroll

on aeStopChangingThumbposition tControl
   if tControl="all" then
      delete variable tAEChangeThumbposition
   else
      delete variable tAEChangeThumbposition[the long id of tControl]
   end if
end aeStopChangingThumbposition

on aeStopChangingHeight tControl
   if tControl="all" then
      delete variable tAEChangeHeight
   else
      delete variable tAEChangeHeight[the long id of tControl]
   end if
end aeStopChangingHeight

on aeStopChangingWidth tControl
   if tControl="all" then
      delete variable tAEChangeWidth
   else
      delete variable tAEChangeWidth[the long id of tControl]
   end if
end aeStopChangingWidth

on aeStopChangingRect tControl
   if tControl="all" then
      delete variable tAEChangeRect
   else
      delete variable tAEChangeRect[the long id of tControl]
   end if
end aeStopChangingRect

on aeStopChangingBackColor tControl
   if tControl="all" then
      delete variable tAEChangeBackColor
   else
      delete variable tAEChangeBackColor[the long id of tControl]
   end if
end aeStopChangingBackColor

on aeStopChangingForeColor tControl
   if tControl="all" then
      delete variable tAEChangeForeColor
   else
      delete variable tAEChangeForeColor[the long id of tControl]
   end if
end aeStopChangingForeColor

on aeStopMorphing tControl
   if tControl="all" then
      delete variable sAEMorphing
   else
      delete variable sAEMorphing[the long id of tControl]
   end if
end aeStopMorphing

--> 4.0 update

on aeScrollTo
   local tControl,tX,tY,tDuration,tMethod,theValue
   if sAEFrameRate is empty then aeResetFrameRate
   repeat with i=1 to paramcount()
      if i<paramcount() then
         put param(i)&"," after theValue
      else
         put param(i) after theValue
      end if
   end repeat
   put item 1 of theValue into tControl
   put item 2 of theValue into tX
   put item 3 of theValue into tY
   put item 4 of theValue into tDuration
   put item 5 of theValue into tMethod
   if there is no tControl then return "error:"&&tControl&&"is not a valid group or field"
   if word 1 of the long name of tControl <> "group" and word 1 of the long name of tControl  <> "field"  then return "error: can only scroll groups or fields"
   if tX is not an integer then return "error:"&&tX&","&tY&&"is not a valid scroll"
   if tY is not an integer then return "error:"&&tX&","&tY&&"is not a valid scroll"
   if tduration is not a number then return "error:"&&tDuration&&"is not a valid duration"
   if tMethod is not among the items of "in,out,inout,bounce,overshoot," then return "error:"&&tDuration&&"is not a valid easing method. Must be in, out, inOut, bounce or overshoot"
   if not sAELockScrolls then
      put tX,tY,tDuration,tMethod,the hscroll of tControl,the vscroll of tControl,the milliseconds into tAeScrolling[the long id of tControl]
      if "aeChangingScroll" is not in the pendingmessages then
         aeChangingScroll
      end if
   else
      put tX,tY,tDuration,tMethod,the hscroll of tControl,the vscroll of tControl,"pending" into tAeScrolling[the long id of tControl]
   end if
end aeScrollTo

on aeLockScrolls
   put true into sAELockScrolls
end aeLockScrolls

on aeUnlockScrolls
   local tNow
   put the milliseconds into tNow
   repeat for each line pControl in the keys of tAEScrolling
      if item 7 of tAEScrolling[pControl] <> "pending" then next repeat
      put tNow into item 7 of tAEScrolling[pControl]
   end repeat
   put false into sAeLockScrolls
   if "aeChangingScroll" is not in the pendingmessages then aeChangingScroll
end aeUnlockScrolls

local sTimeTakenToChangeScroll

/**

Summary: Animates the scroll property of the target object
*/
command aeChangingScroll
   --put the millisecs
   lock screen
   local tTimeLost,tFramesLost,tElapsed
   local tDuration,tMethod,tStartX,tDestX,tDestY,tStartY,tStartTime,tX,tY,tEndX
   if sAEFrameRate is not a number then aeResetFrameRate
   put the milliseconds into tElapsed
   put the milliseconds - sTimeTakenToChangeScroll into tTimeLost
   put tTimeLost - (1000/ sAEFrameRate) into tTimeLost
   put ((1000/sAeFramerate) + tTimeLost) into tFramesLost
   if tFramesLost<>0 then
      put 1000/tFramesLost into tFramesLost
   end if
   if tFramesLost>sAEFrameRate then put sAEFrameRate into tFramesLost
   put tFramesLost into sAERealFrameRate
   put the millisecs into sTimeTakenToChangeScroll
   --lock screen
   repeat for each line pControl in the keys of tAEScrolling
      send "aeEnterFrame" && "aeScrollTo" to pcontrol
      put item 1 of tAEScrolling[pControl] into tDestX
      put item 2 of tAEScrolling[pControl] into tDestY
      put item 3 of tAEScrolling[pControl] into tDuration
      put item 4 of tAEScrolling[pControl] into tMethod
      put item 5 of tAEScrolling[pControl] into tStartX
      put item 6 of tAEScrolling[pControl] into tStartY
      put item 7 of tAEScrolling[pControl] into tStartTime
      if tStartTime="pending" then next repeat
      switch tMethod
         case "in"
            put aeEaseIn(tStartX,tDestX,tDuration,the milliseconds-tStartTime,3) into tX
            put aeEaseIn(tStartY,tDestY,tDuration,the milliseconds-tStartTime,3) into tY
            break
         case "out"
            put aeEaseOut(tStartX,tDestX,tDuration,the milliseconds-tStartTime,3) into tX
            put aeEaseOut(tStartY,tDestY,tDuration,the milliseconds-tStartTime,3) into tY
            break
         case "inOut"
            put aeEaseInOut(tStartX,tDestX,tDuration,the milliseconds-tStartTime,3) into tX
            put aeEaseInOut(tStartY,tDestY,tDuration,the milliseconds-tStartTime,3) into tY
            break
         case "bounce"
            put aeBounceEaseOut(tStartX,tDestX,tDuration,the milliseconds-tStartTime) into tX
            put aeBounceEaseOut(tStartY,tDestY,tDuration,the milliseconds-tStartTime) into tY
            break
         case "overshoot"
            put aeOverShootEaseOut(tStartX,tDestX,tDuration,the milliseconds-tStartTime) into tX
            put aeOverShootEaseOut(tStartX,tDestY,tDuration,the milliseconds-tStartTime) into tY
            break
         default
            put aeEaseIn(tStartX,tDestX,tDuration,the milliseconds-tStartTime,1) into tX
            put aeEaseIn(tStartY,tDestY,tDuration,the milliseconds-tStartTime,1) into tY
            break
      end switch
      if the milliseconds-tStartTime<tDuration then
         set the hscroll of pControl to round(tX)
         set the vscroll of pControl to round(tY)
      else
         set the hscroll of pControl to round(tDestX)
         set the vscroll of pControl to round(tDestY)
         delete variable tAEScrolling[pControl]
         send "aeChangeScrollDone" to pControl
      end if
      send "aeExitFrame" && "aeScrollTo" to pControl
   end repeat
   --   unlock screen
   unlock screen
   put the milliseconds-tElapsed into tElapsed
   if the keys of tAEScrolling is not empty then
      if tElapsed < 1000 / sAEFrameRate then
         if "aeChangingScroll" is not in the pendingmessages then
            send "aeChangingScroll" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeChangingScroll" is not in the pendingmessages then
            send "aeChangingScroll" to me in 5 millisecs
         end if
      end if
   end if
end aeChangingScroll

on aeChangeScrollDone
   
end aeChangeScrollDone

local aeCollisionListener,aelistenForCollisions

setprop aeListenForCollisionsWith pObjectList
   local tInvalidControls,tObjects
   repeat for each line theLine in pObjectList
      if there is no theLine then
         put theLine & cr after tInvalidControls
         next repeat
      end if
      put theLine & cr after tObjects
   end repeat
   delete char -1 of tObjects
   delete char -1 of tInvalidControls
   put tObjects into aeCollisionListener[the long name of the target]
   if tInvalidControls is not empty then
      return tInvalidControls
   end if
end aeListenForCollisionsWith

on aeStartListeningForCollisions
   put true into aeListenForCollisions
   if aeListenForCollisions is not in the pendingmessages then
      aeListenForCollisions
   end if
end aeStartListeningForCollisions

on aeStopListeningForCollisions
   put false into aeListenForCollisions
   repeat for each line theLine in the pendingmessages
      if "aeListenForCollisions" is in theLine then cancel item 1 of theLine
   end repeat
   delete variable aeCollisionListener
end aeStopListeningForCollisions

on aeListenForCollisions
   try
      if sAEFrameRate is empty then aeSetFrameRate 25
      local tObjects,tElapsed
      put the millisecs into tElapsed
      if not aeListenForCollisions then exit aeListenForCollisions
      repeat for each key theKey in aeCollisionListener
         put empty into tObjects
         repeat for each line theLine in aeCollisionListener[theKey]
            if intersect(theKey,theLine,"pixels") then
               put theLine&cr after tObjects
            end if
         end repeat
         delete char -1 of tObjects
         if tObjects is not empty then
            dispatch "aeCollision" to theKey with tObjects
         end if
      end repeat
      put the millisecs - tElapsed into tElapsed
      if tElapsed < 1000 / sAEFrameRate then
         if "aeListenForCollisions" is not in the pendingmessages then
            send "aeListenForCollisions" to me in (1000/sAEFRameRate)- tElapsed millisecs
         end if
      else
         if "aeListenForCollisions" is not in the pendingmessages then
            send "aeListenForCollisions" to me in 5 millisecs
         end if
      end if
   catch theErr
      throw theErr
   end try
end aeListenForCollisions

on aeCollision pObjects

end aeCollision

/**

Summary: Returns a list of objects colliding with the target

Returns: The computed result
*/
function aeCollidingObjects
   local tObjects,tReturn
   repeat for each key theKey in aeCollisionListener
      put empty into tObjects -- 5.0.4 fix for errant collision lists
      repeat for each line theLine in aeCollisionListener[theKey]
         if intersect(theKey,theLine,"pixels") then
            put theLine&cr after tObjects
         end if
      end repeat
      delete char -1 of tObjects
      put tObjects into tReturn[theKey]
      if tReturn[theKey] is empty then delete variable tReturn[theKey]
   end repeat
   return tReturn
end aeCollidingObjects

-- contribution by Geoff Canyon

/**

Summary: Rotates a list of 3D points around the X, Y and Z axes

Parameters:
pointList: A list of 3D points in the form of "x,y,z" (one point per line)
pXRotation: The rotation around the X axis in degrees
pYRotation: The rotation around the Y axis in degrees
pZRotation: The rotation around the Z axis in degrees
pFocalLength: The focal length for perspective projection (optional, default is 300)

Returns: The computed result
*/
function rotate3DPointList pointList,pXRotation,pYRotation,pZRotation,pFocalLength
   local cX,cY,cZ,i
   local newX,newY,newZ
   local pX,pY
   local pZ,tScaleFactor,sX
   local sY,sZ,theValue,xY
   local xZ,yX,yZ,zX
   local zY
local tReturnList
   if pFocalLength is empty then put 300 into pFocalLength
   put pXrotation*pi/180 into pXrotation
   put pYrotation*pi/180 into pYrotation
   put pZrotation*pi/180 into pZrotation
   put sin(pXRotation) into sX
   put cos(pXRotation) into cX
   put sin(pYRotation) into sY
   put cos(pYRotation) into cY
   put sin(pZRotation) into sZ
   put cos(pZRotation) into cZ
   try
      repeat for each line L in pointList
         if L is empty then
            put cr after tReturnList
            next repeat
         end if
         put item 1 of L into pX
         put item 2 of L into pY
         put item 3 of L into pZ
         -- rotation around x
         put cx*pY - sx*pZ into xY
         put sx*pY + cx*pZ into xZ
         -- rotation around y
         put cy*xz - sy*pX into yZ
         put sy*xz + cy*pX into yX
         -- rotation around z
         put cz*yx - sz*xy into zX
         put sz*yx + cz*xy into zY
         put pfocalLength/(pfocalLength + yz) into tScaleFactor
         put zX * tScalefactor into newX
         put zY * tScalefactor into newY
         put yZ into newZ
         put newX,newY,newZ & cr after tReturnList
      end repeat
      return char 1 to - 2 of tReturnList
   catch theError
      return theError
   end try
end rotate3DPointList

/**

Summary: Projects a list of 3D points onto the 2D screen

Parameters:
pointList: A list of 3D points in the form of "x,y,z" (one point per line)
pOriginX: The x-coordinate of the origin
pOriginY: The y-coordinate of the origin
pFocalLength: The focal length for perspective projection (optional, default is 300)

Returns: The computed result
*/
function ae3dConvertListToScreen pointList,pOriginX,pOriginY,pFocalLength
   local i
   local pX,pY,pZ,theValue
   local tScaleFactor,tXCartesian,tYCartesian,tReturnList
   if pFocalLength is empty then put 300 into pFocalLength
   try
      repeat for each line L in pointList
         if L is empty then
            put cr after tReturnList
            next repeat
         end if
         put item 1 of L into pX
         put item 2 of L into pY
         put item 3 of L into pZ
         put pFocalLength/(pFocalLength+pZ) into tScaleFactor
         put pX*tScaleFactor into tXCartesian
         put pY*tScaleFactor into tYCartesian
         put round(tXCartesian+pOriginX),round(-tYCartesian+pOriginY) & cr after tReturnList
      end repeat
      return char 1 to -2 of tReturnList
   catch theError
      return theError
   end try
end ae3dConvertListToScreen

