#!/usr/bin/python

# A python script to dump to stdout the data recieved on serial port.
# Tested on Max OSX 10.8.5 with python 2.7.2.
#
# Based on example from pySerial.
#
# NOTE: Opening the serial port may cause a DTR level change that on
# some Arduinos cause a CPU reset.

import os
import termios
import sys
import select

# TODO: make these command line args.
speed = 115200
port = '/dev/cu.usbserial-A600dOYP'

# Open port in read only mode. Call is blocking.
fd= os.open(port, os.O_RDONLY | os.O_NOCTTY )

# Setup the port.
iflag, oflag, cflag, lflag, ispeed, ospeed, cc = termios.tcgetattr(fd) 
ispeed = speed
ospeed = speed
termios.tcsetattr(fd, termios.TCSANOW, [iflag, oflag, cflag, lflag, ispeed, ospeed, cc])

# Read in a loop
# TODO: buffer lines and process one line at a time.
while True:
  # Wait for rx data ready. Timeout of 0 indicates wait forever.
  ready,_,_ = select.select([fd],[],[], 0)
  # Read and output one character
  buf = os.read(fd, 1)
  sys.stdout.write(buf)
  sys.stdout.flush()

os.close(fd)




