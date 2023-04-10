dim x
dim n

n = int(timer/100)
x = timer - n*100

SetRegisterValue 3, 0, n
SetRegisterValue 3, 1, x

SetRegisterValue 3, 3, int(x/10)
SetRegisterValue 3, 4, int(x/10)*10
SetRegisterValue 3, 5, int(x/10)* 10 -x


'if ((int(x/10)*10 - x) > -0.5) then
'Or
'if (Cint(int(x/10)*10 - x)=0) then
'Or
if (Cint(int(x/10)*10) = Cint(x)) then
 SetRegisterValue 3, 2, x
end if

x = GetRegisterValue(3, 11)
SetRegisterValue 3, 10, x

x = Second(Now)
SetRegisterValue 3, 20, x

x = GetLastRunTime
SetRegisterValue 3, 30, x