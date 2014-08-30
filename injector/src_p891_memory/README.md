LINBUS INJECTOR FORK - PORSCHE 981 - MEMORY
===========================================

This directory contains the custom Linbus Injector source code for Porsche 981 that was 
contributed by John Miles (john (at) miles.io).  It is based on the reference source code 
in the Arduino directory with the following modifications:

* Compilation is done using the avr tool chain without dependency on the Arduino IDE and libraries.

* This version has code to handle all of the possible buttons in the center console for a 981 via the injector board, and it should handle all of the same buttons in the 991 as well. These include auto start/stop, PASM, PSE, PSM, top up, top down, spoiler, Sport, and Sport Plus. The LED state is also readable for each of the buttons that has one (i.e., all but the top up/down buttons.)

* Sport and PSE are remembered independently between drive cycles. Manually pressing the Sport or PSE button causes the code to read the feature state (via the LED) and store it in nonvolatile EEPROM. Beginning 1000 ms after startup, any discrepancy between the Sport or PSE LED states and the corresponding stored settings in EEPROM causes the code to inject a button press. This means that PSE no longer comes on automatically when you select Sport mode, and no longer turns off automatically when you go back to normal mode.

* The code doesn't preserve any other button states besides Sport and PSE. It doesn't restore the Sport Plus state because most sane people won't want to shift near redline on a cold engine the next time they drive the car. (At the same time, it's smart enough not to try to restore Sport mode if you cause its LED to turn off by manually selecting Sport Plus.) The PASM and spoiler buttons are already remembered if I'm not mistaken, auto-start/stop is already remembered on US cars, and I don't personally ever turn PSM off so I didn't look into handling it.

Ideas for further work:

* The microcontroller could enter low-power mode after ignition shutoff. Currently (no pun intended) the module draws about 20 mA at all times, which is more than I'd prefer if I were going to let the car sit for a couple of weeks with no battery tender.

* Emulate/replace the functionality of a SmartTop. One-touch top up/top down is handled by a SmartTop module in my car, but the code could easily be modified to do that as well. However, the LIN bus segment in the console doesn't seem to have access to key fob input, unlike the one under the driver's seat where the SmartTop is located.

* Test on a 991. I think I've seen PDCC buttons on at least some 991s, so that button and LED might need to be added to the code. Not 100% sure what other buttons might be present on various 991 variants (Targa, Turbo...) 

Build notes:

*  Since the Arduino IDE includes the AVR tool chain, one simple way to install the tool chain is to install the Arduino IDE.
*  The project is built using a makefile that's compatible with NMAKE.EXE from MS Visual Studio.  GNU Make will probably handle it as well with minor modifications.

 
