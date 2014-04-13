// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef I2C_H
#define I2C_H

#include "avr_util.h"

// I2C master API.
namespace i2c {
  
// LSB bit of start byte. Determine data transfer direction.
namespace direction {
  const uint8 READ = 1;
  const uint8 WRITE = 0;
}

// Called once from the main setup().
extern void setup();

// Start a transaction. Can be used also for repeated start.
// Return true iff error.
extern bool start(uint8 address_and_direction);

// Terminate the transaction.
extern void stop();

// Send one byte.  Returns true if an error.
extern bool writeByte(uint8 data);

// Read one byte, request more. Returns the byte read.
// TODO: return also status.
extern uint8 readByteWithAck();

// Read last byte. Will follow with a stop condition. Returns the byte read.
// TODO: return also status
extern uint8 readByteWithNak();

}  // namespace i2c

#endif
