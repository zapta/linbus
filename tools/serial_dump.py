#!/usr/bin/python

# A python script to dump to stdout the data recieved on serial port.
# Tested on Max OSX 10.8.5 with python 2.7.2.
#
# Based on example from pySerial.
#
# NOTE: Opening the serial port may cause a DTR level change that on
# some Arduinos cause a CPU reset.

import os
import select
import sys
import termios
import time

# TODO: make these command line args.
speed = 115200
port = "/dev/cu.usbserial-A600dOYP"

# Wait for next input character and return it as a single
# char string.
def read_char(fd):
  # Wait for rx data ready. Timeout of 0 indicates wait forever.
  ready,_,_ = select.select([fd],[],[], 0)
  # Read and output one character
  return os.read(fd, 1)

# Read an return a single line, without the eol charcater.
def read_line(fd):
  line = [] 
  while True:
    char = read_char(fd)
    if (char == "\n"):
      return "".join(line)
    line.append(char)

# Return time now in millis. We use it to comptute relative time.
def time_millis():
  return int(round(time.time() * 1000))

# Open the serial port for reading at the specified speed.
# Returns the port's fd.
def open_port():
  # Open port in read only mode. Call is blocking.
  fd = os.open(port, os.O_RDONLY | os.O_NOCTTY )

  # Setup the port.
  iflag, oflag, cflag, lflag, ispeed, ospeed, cc = termios.tcgetattr(fd) 
  ispeed = speed
  ospeed = speed
  termios.tcsetattr(fd, termios.TCSANOW, [iflag, oflag, cflag, lflag, ispeed, ospeed, cc])
  return fd

def main():
  fd = open_port()
  start_time_millis = time_millis()
  while True:
    line = read_line(fd);
    rel_time_millis = time_millis() - start_time_millis
    seconds = int(rel_time_millis / 1000)
    millis = rel_time_millis % 1000
    out_line = "%05d.%03d %s\n" % (seconds, millis, line)
    sys.stdout.write(out_line)
    sys.stdout.flush()

main()

