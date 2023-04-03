dim x
dim n

n = int(timer/100)
x = timer - n*100

SetRegisterValue 3, 0, n
SetRegisterValue 3, 1, x

if ((int(x/10)*10 - x) >= -1) then
 SetRegisterValue 3, 2, x
end if

