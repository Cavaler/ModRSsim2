dim x
dim n

n = int(timer/100)
x = GetRegisterValue (3, 0)

'SetRegisterValue 3, 0, n
SetRegisterValue 3, 1, x

'if ((int(x/10)*10 - x) >= -1) then
' SetRegisterValue 3, 2, x
'end if


AddDebugString ("test one")