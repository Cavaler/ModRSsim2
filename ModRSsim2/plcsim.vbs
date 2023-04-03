'---------------------------------------------------------------
' Data simulation using modRSim
'
' Note that all subroutines / functions must appear before the 
' subroutine is called.
'
' Note: Addressing in the automtion calls is Off by one. So,
' when you want to read/modify value of holding register 305,
' you will need to pass 304 as the register address.
'---------------------------------------------------------------

'---------------------------------------------------------------
' Routine to simulate saw tooth data pattern. The value of specified
' holding register will increase from minValue to maxValue, and 
' upon reaching maxValue it will drop to minValue
'---------------------------------------------------------------
Sub SimulateSawTooth(reg, minValue, maxValue) 
Dim hrValue

   hrValue = GetRegisterValue(3, reg)
   if (hrValue > maxValue) then
	   hrValue = minValue
   else
	   hrValue = hrValue + 1
   end if
   SetRegisterValue 3, reg, hrValue
End Sub

'---------------------------------------------------------------
' Routine to simulate correlated random variations in specified
' holding register value.
'---------------------------------------------------------------
Sub RandomVariation(reg, minVal, maxVal, noiseLevel)
Dim hrValue
Dim minChg
Dim maxChg
Dim chg

   minChg = -noiseLevel
   maxChg = noiseLevel
   
   hrValue = GetRegisterValue(3, reg)

   chg = Int((maxChg-minChg+1)*Rnd+minChg)
   hrValue = hrValue + chg

   if (hrValue < minVal) then
	   hrValue = minVal
   end if

   if (hrValue > maxVal) then
	   hrValue = maxVal
   end if

   SetRegisterValue 3, reg, hrValue
End Sub

'--------------------- Execution portion -----------------------
Dim reg

dim hrStart
dim hrEnd

dim coilStart
dim coilEnd
Dim coilValue

' Holding registers - Randomly modified the holding register values in 
' the specified register range
'
' Note: using too large a range can cause disconnection
'
'hrStart = 302 ' 40002
'hrEnd = 360   ' 40100

'for reg = hrStart to hrEnd
'   call RandomVariation(reg, 100, 150)
'next

' Simulate saw tooth input on 40302
Call SimulateSawTooth(301, 15, 50) 
Call SimulateSawTooth(316, 10, 40) 
'Call SimulateTriangular(331, 20, 60) 
'Call SimulateTriangular(346, 30, 70) 

'call RandomVariation(301, 10, 50)
'call RandomVariation(316, 10, 50)
call RandomVariation(331, 10, 50, 3)
call RandomVariation(346, 10, 50, 2)

'Coils - Randomly toggles coil values in the specified register range
'coilStart = 0 ' 001
'coilEnd = 100 ' 100

'   reg = Int((coilEnd - coilStart) * Rnd+coilStart)

   '0 indicates holding registers
'   coilValue = GetRegisterValue(0, reg)
'   if (coilValue = 1) then
'	   coilValue = 0
'   else
'	   coilValue = 1
'   end if
'   SetRegisterValue 0, reg, coilValue

