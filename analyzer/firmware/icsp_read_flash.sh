#!/bin/bash -x
#
# A command line script to read the flash of a analyzer board via the ICSP connector.
# Requires an installed avrdude software and having a working programmer.

# See avrdude command line options here http://www.nongnu.org/avrdude/user-manual/avrdude.html

# Set this to the code of the programmer you have. See above link for programmer codes.
PROGRAMMER_CODE="avrispmkII"

avrdude \
  -B 4 \
  -c ${PROGRAMMER_CODE} \
  -p m328p \
  -U flash:r:_analyzer_flash.hex:i

