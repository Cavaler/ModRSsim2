dim x
dim n
dim runtime
dim station

n=0
for n=1 to 5000
   x = getregistervalue(3,n)
   SetRegisterValue 3, n, x+1
next
runtime = Getlastruntime 
SetRegisterValue 3, 0, runtime

if (StationEnabled (station)) then
   Disablestation station
else
   enablestation station
end if

SetRegisterValue 3, 242, station

station = station + 1
if station > 12 then station = 0
