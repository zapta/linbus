#!/usr/bin/python

# A python script to dump to stdout the data recieved on serial port.
# Tested on Max OSX 10.8.5 with python 2.7.2.
#
# Requires installation of the PySerial library. INSTALLATION.txt for details.
#
# NOTE: Opening the serial port may cause a DTR signal change that may reset 
# the analyzer board.

import getopt
import optparse
import os
import re
import select
import serial
import sys
import traceback
import time

# Set later when parsing args.
FLAGS = None

# Pattern to parse a frame line.
# NOTE: excluding frames with ERR suffix.
kFrameRegex = re.compile('^([0-9a-f]{2})((?: [0-9a-f]{2})+) ([0-9a-f]{2})(?: [*])?$')

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
    print "Ignoring: [%s]" % line
    return None
  return LinFrame(m.group(1), m.group(2).split(), m.group(3))

# Parse args and set FLAGS.
def parseArgs(argv):
  global FLAGS
  parser = optparse.OptionParser()
  parser.add_option(
      "-p", "--port", dest="port",
      default="/dev/cu.usbserial-AM01VGNC",
      help="serial port to read", metavar="PORT")
  parser.add_option(
      "-d", "--diff", dest="diff",
      default=False,
      help="show only data changes")
  parser.add_option(
      "-s", "--speed", dest="speed",
      default=115200,
      help="use this serial port baud rate")
  (FLAGS, args) = parser.parse_args()
  if args:
    print "Uexpected arguments:", args
    print "Aborting"
    sys.exit(1)
  print "Flags:"
  print ("  --port ..........[%s]" % FLAGS.port)
  print ("  --speed .........[%s]" % FLAGS.speed)
  print ("  --diff ..........[%s]" % FLAGS.diff)

# Return time now in millis. We use it to comptute relative time.
def timeMillis():
  return int(round(time.time() * 1000))

# Format relative time in millis as "sssss.mmm".
def formatRelativeTimeMillis(millis):
  seconds = int(millis / 1000)
  millis_fraction = millis % 1000
  return "%05d.%03d" % (seconds, millis_fraction)

# Read and return a single line, without the terminating EOL char.
#def readLine(serial_port):
#  line = serial_port.readline().rstrip('\n')
#  return line.rstring('\n')

# Open the serial port for reading at the specified speed.
# Returns the opened serial port.
def openPort():
  try:
    print "\nOpening port:", FLAGS.port
    # First opening with zero read timeout (non blocking).
    serial_port = serial.Serial(
      FLAGS.port, 
      FLAGS.speed, 
      bytesize = serial.EIGHTBITS, 
      parity = serial.PARITY_NONE, 
      stopbits = serial.STOPBITS_ONE, 
      timeout = 0)
    # Read pending chars. This cleans left over bytes.
    while (True):
      b = serial_port.read()
      if not b:
        break
    # Make serial port blocking for read.
    serial_port.timeout = None
    print "Done\n"
  except:
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60
    print ("Failed to open port %s, aborting" % FLAGS.port)
    sys.exit(1)
  return serial_port

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
  serial_port = openPort()
  start_time_millis = timeMillis();
  last_bit_lists = {}
  while True:
    line = serial_port.readline().rstrip('\n')
    rel_time_millis = timeMillis() - start_time_millis
    timestamp = formatRelativeTimeMillis(rel_time_millis);
    # Dump raw lines
    if not FLAGS.diff:
      out_line = "%s  %s\n" % (timestamp, line)
      sys.stdout.write(out_line)
      sys.stdout.flush()
      continue
    # Parse and dump diffs only
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
    diff_bit_list = diffBitLists(old_bit_list, new_bit_list)
    diff_str = insertSeperators("".join(diff_bit_list), 4, " ")
    diff_str = insertSeperators(diff_str, 10, "| ")
    out_line = "%s  %s: | %s |\n" % (timestamp, id, diff_str)
    sys.stdout.write(out_line)
    sys.stdout.flush()

if __name__ == "__main__":
  main(sys.argv[1:])


