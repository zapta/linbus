Arduino Linbus Injector board programming.

Use AVR ICSP programmer such as AVR ISP MKII and programming software
such as AVRDUDE or AVRDUDES. Many other programmers and programming
software will also work.

Use the 2x3 ICSP programming header on the Injector board. Make sure you
orient it correctly (connect from the top side, pay attention to pin 1).
You can solder header pins or use pogo pins like Sparkfun's 
ISP Pogo Adapter. Other adapters like Hobbyking's Atmega Socket Firmware 
Flashing Tool may also work.

MCU type: ATmeta328P

Fuses:
L  : 0xFF
H  : 0xDA
E  : 0x05
LB : 0x3F

Flash image: injector_flash.hex

The image includes an Arduino Mini Pro bootloader so once
programmed, you can program software changes directly from
the Arduino IDE (board = Arduino Pro Mini 5V 16Mhz) using
a serial to USB adapter such as Sparkfun FTDI Basic.


