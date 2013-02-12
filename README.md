Badge-Test-Code
===============
1.All the code here is from openbeacon except an added section in main.c under MSP430_tag directory. Find the do while loop. 
This is where all the checking code lives in. The code currently in the loop is meant to check the joint between the cpu and the 
accelerometer. This code may not work so you must check.

2.The code in comments in the loop works fine. Basically if you run the code and you see LED2 light up then that means at least one of the pins
from the SPI is not connected properly .

3.debug1.ino will let you check whether your switch is working. The LED on board should toggle along with the switch.