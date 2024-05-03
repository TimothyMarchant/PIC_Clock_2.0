This is my second attempt at making a PIC clock it uses all 12 I/O pins.  The original used the PIC16F18313 which only had 6 I/O pins.  This one is different since it uses the MCP7940M real time clock with I2C.  Surprisingly this one uses less program memory unlike the first one (both chips have 3.5 kb of programmable memory).  

The actual circuitry is very similiar to the original clock, but this one does not use the SN74HC165N shift register.  So the actual supplies I used didn't really change.
So the supplies I ended up using were the following (excludes wires).

    1 PIC16f18323

    1 7-segment display (I used this one https://www.mouser.com/ProductDetail/485-1001)

    4 PN2222TF transistors (NPN)

    4 0.1 uF capacitors (two for the shift register, two for the minimum circuitry for the PIC)

    5 22k OHM resistors (4 are used with the transistors and one is used for the colon of the 7-segment display).

    9 10k OHM resistors (one is used for the minimum circuitry for the PIC)

    10 10k/4.7k ohm resistors (either value works, for the buttons I use 4.7k because that's what I had out).

    1 100-470 ohm resistor for minimum circuitry for the PIC

    6 buttons.

    2 SN74HC595N Shift Registers

    1 MCP7940M real time clock.
I will upload a schematic and potential PCB design at some point.  

NOTE: if you want to use this, something to note is that RA3 will always be held HIGH or LOW if you leave the MCLRE pin plugged into your pickit (held in reset vs not in reset).  So once you've uploaded the program unplug the MCLRE wire connected to your pickit (assuming you're using one).  When I was working on this, I didn't know this, so hopefully you won't run into this problem.  
