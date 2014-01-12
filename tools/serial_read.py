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
import re
import select
import sys
import termios
import time

# TODO: make these flags
kPrintRawLines = False
kPrintFrameDiffs = True

# TODO: make these command line args.
FLAG_speed = 115200
FLAG_port = "/dev/cu.usbserial-A600dOYP"

# For getopt parsing.
kFlagNames = [
  "speed=",
  "port="
]

kUsage = [
  "serial_read.py [option ...]",
  "Options:",
  "--port=<port>",
  "--speed=<speed>"
]

# NOTE: excluding frames with ERR suffix.
kFrameRegex = re.compile('^([0-9a-f]{2})((?: [0-9a-f]{2})+) ([0-9a-f]{2})$')

# Represents a parsed LIN frame
class LinFrame:
  def __init__(self, id, data, checksum):
    self.id = id
    self.data = data
    self.checksum = checksum
 
  def __str__(self):
    return "[" + self.id + "] [" + " ".join(self.data) + "] [" + self.checksum + "]"

  __repr__ = __str__

# Convert a list of two hex digits strings to a list of a single binary digit
# strings.
def hexListToBitList(hex_list):
  result = []
  for hex_str in hex_list:
    as_int = int(hex_str, 16)
    bin_str = bin(as_int)[2:].zfill(8)
    result.extend(list(bin_str))
  return result

# If a valid frame return a LinFrame, otherwise 
# returns None.
def parseLine(line):
  m = kFrameRegex.match(line)
  if not m:
    return None
  return LinFrame(m.group(1), m.group(2).split(), m.group(3))

# Parse args and set flags.
def parseArgs(argv):
  global FLAG_speed
  global FLAG_port
  try:
    opts, args = getopt.getopt(argv, "", kFlagNames)
  except getopt.GetoptError:
    print "\n".join(kUsage)
    sys.exit(2)
  for opt, arg in opts:
    if opt == "--port":
      FLAG_port = arg
    elif opt == "--speed":
      FLAG_speed = int(arg)
  print "---"
  print "Port:  ", FLAG_port
  print "Speed: ", FLAG_speed
  print "---"

# Return time now in millis. We use it to comptute relative time.
def timeMillis():
  return int(round(time.time() * 1000))

# Format relative time in millis as "sssss.mmm".
def formatRelativeTimeMillis(millis):
  seconds = int(millis / 1000)
  millis_fraction = millis % 1000
  return "%05d.%03d" % (seconds, millis_fraction)

# Clear pending chars until no more.
def clearPendingChars(fd):
  while (True):
    ready,_,_ = select.select([fd],[],[], 1)
    if not ready:
      return;
    os.read(fd, 1)

# Wait for next input character and return it as a single
# char string.
def readChar(fd):
  # Wait for rx data ready. Timeout of 0 indicates wait forever.
  ready,_,_ = select.select([fd],[],[], 0)
  # Read and output one character
  return os.read(fd, 1)

# Read and return a single line, without the eol charcater.
def readLine(fd):
  line = [] 
  while True:
    char = readChar(fd)
    if (char == "\n"):
      return "".join(line)
    line.append(char)

# Open the serial port for reading at the specified speed.
# Returns the port's fd.
def openPort():
  # Open port in read only mode. Call is blocking.
  fd = os.open(FLAG_port, os.O_RDONLY | os.O_NOCTTY )

  # Setup the port.
  iflag, oflag, cflag, lflag, ispeed, ospeed, cc = termios.tcgetattr(fd) 
  ispeed = FLAG_speed
  ospeed = FLAG_speed
  termios.tcsetattr(fd, termios.TCSANOW, [iflag, oflag, cflag, lflag, ispeed, ospeed, cc])
  clearPendingChars(fd)
  return fd

# For now, assuming both are of same size.
# Return a list of '0', '1' and '-', one per data bit.
def diffBitLists(old_bit_list, new_bit_list):
  result = []
  for idx, new_val in enumerate(new_bit_list):
    if new_val != old_bit_list[idx]:
      result.append(new_val)
    else:
      result.append("-")
  return result

# Insert a seperator every n  charaters.
def insertSeperators(str, n, sep):
  return sep.join(str[i: i+n] for i in range(0, len(str), n))

def main(argv):
  parseArgs(argv)  
  fd = openPort()
  start_time_millis = timeMillis();
  last_bit_lists = {}
  while True:
    line = readLine(fd);
    rel_time_millis = timeMillis() - start_time_millis
    timestamp = formatRelativeTimeMillis(rel_time_millis);
    if kPrintRawLines:
      out_line = "%s  %s\n" % (timestamp, line)
      sys.stdout.write(out_line)
      sys.stdout.flush()
    frame = parseLine(line)
    if not frame:
      continue
    id = frame.id
    new_bit_list = hexListToBitList(frame.data)
    if id not in last_bit_lists:
      last_bit_lists[id] = new_bit_list
      continue
    old_bit_list = last_bit_lists[id]
    last_bit_lists[id] = new_bit_list
    if new_bit_list == old_bit_list:
      continue
    if kPrintFrameDiffs:
      diff_bit_list = diffBitLists(old_bit_list, new_bit_list)
      diff_str = insertSeperators("".join(diff_bit_list), 4, " ")
      diff_str = insertSeperators(diff_str, 10, "| ")
      out_line = "%s  %s: | %s |\n" % (timestamp, id, diff_str)
      sys.stdout.write(out_line)
      sys.stdout.flush()

if __name__ == "__main__":
  main(sys.argv[1:])


