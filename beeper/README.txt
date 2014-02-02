The Linbus Beeper is a small board (1.15" x 1.57") that is attached
to a linbus signal of a car and sounds a beep when certain conditions
are met.

The default firmware triggers the beeps when a frame that matches
the following conditions is detected (this happened to indicate
a reverse gear on my car)

Frame id = 39
Data size = 6
Bit 3 of the first data byte is set.

Example frame: [39] [04 00 00 00 00 00]

The board also dumps all the linbus frames it detects to a serial
port (115,200 baud) which can be connected to a computer using a
USB/serial adapter. Note however that the ground of the serial 
signal is shared with the linbus (that is, the car's) ground so make
sure not to create ground loop (a portable computer powered by
batteries should be fine).

