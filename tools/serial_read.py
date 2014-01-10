#!/usr/bin/python

# A python script to dump to stdout the data recieved on serial port.
# Tested on Max OSX 10.8.5 with python 2.7.2.
#
# Based on example from pySerial.
#
# NOTE: Opening the serial port may cause a DTR level change that on
# some Arduinos cause a CPU reset.

import getopt
import os
import select
import sys
import termios
import time

# TODO: make these command line args.
flag_speed = 115200
flag_port = "/dev/cu.usbserial-A600dOYP"

# For getopt parsing.
flag_names = [
  "speed=",
  "port="
]

usage = [
  "serial_read.py [--port=<port>] [--speed=<speed>]"
]

# Parse args and set flags.
def parseArgs(argv):
  global flag_speed
  global flag_port
  try:
    opts, args = getopt.getopt(argv, "", flag_names)
  except getopt.GetoptError:
    print "\n".join(usage)
    sys.exit(2)
  for opt, arg in opts:
    if opt == "--port":
      flag_port = arg
    elif opt == "--speed":
      flag_speed = int(arg)
  print "---"
  print "Port:  ", flag_port
  print "Speed: ", flag_speed
  print "---"

# Return time now in millis. We use it to comptute relative time.
def time_millis():
  return int(round(time.time() * 1000))

# Format relative time in millis as "sssss.mmm".
def format_relative_time_millis(millis):
  seconds = int(millis / 1000)
  millis_fraction = millis % 1000
  return "%05d.%03d" % (seconds, millis_fraction)

# Clear pending chars until no more.
def clear_pending_chars(fd):
  while (True):
    ready,_,_ = select.select([fd],[],[], 1)
    if not ready:
      return;
    os.read(fd, 1)

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
    # Ignore CR. The Arduino Print class seems to append
    # them to '\n'.
    if (char == "\r"):
      continue
    if (char == "\n"):
      return "".join(line)
    line.append(char)

# Open the serial port for reading at the specified speed.
# Returns the port's fd.
def open_port():
  # Open port in read only mode. Call is blocking.
  fd = os.open(flag_port, os.O_RDONLY | os.O_NOCTTY )

  # Setup the port.
  iflag, oflag, cflag, lflag, ispeed, ospeed, cc = termios.tcgetattr(fd) 
  ispeed = flag_speed
  ospeed = flag_speed
  termios.tcsetattr(fd, termios.TCSANOW, [iflag, oflag, cflag, lflag, ispeed, ospeed, cc])
  clear_pending_chars(fd)
  return fd

def main(argv):
  parseArgs(argv)  
  fd = open_port()
  start_time_millis = None
  while True:
    line = read_line(fd);
    # First line starts with timestamp of 0.
    # Note that the timestamp represents the time the end
    # of the line arrived to this program, not the start
    # time of the LIN frame. It is used for reference only.
    if not start_time_millis:
      start_time_millis = time_millis()
      rel_time_millis = 0
    else:
      rel_time_millis = time_millis() - start_time_millis
    timestamp = format_relative_time_millis(rel_time_millis);
    out_line = "%s  %s\n" % (timestamp, line)
    sys.stdout.write(out_line)
    sys.stdout.flush()

if __name__ == "__main__":
  main(sys.argv[1:])


