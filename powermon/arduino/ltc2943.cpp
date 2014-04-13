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

#include "ltc2943.h"

#include "i2c.h"

namespace ltc2943 {
  
// The I2C address byte to access the LTC294. This is hard coded in the 
// LTC2943.
static const uint8 kLtc2943AddressByte = (0x64 << 1);

// Control register fields.
static const uint8 kLtc2943AutomaticMode = 0xc0;
static const uint8 kLtc2943PrescalarX1   = 0x00;
static const uint8 kLtc2943DisableAlccPin = 0x00;

void setup() {
  i2c::setup();
}

bool init() {
  const uint8 control_mode = kLtc2943AutomaticMode | kLtc2943PrescalarX1 | kLtc2943DisableAlccPin ;              
  return writeReg8(regs8::kControl, control_mode);
}

// Write a value to a LTC2943 8 bit register. Returns true if ok.
bool writeReg8(uint8 reg,  uint8 value) {
  bool error = false;
  error |= i2c::start(kLtc2943AddressByte | i2c::direction::WRITE);     
  error |= i2c::writeByte(reg);                       
  error |= i2c::writeByte(value);                      
  i2c::stop(); 
  return !error;  
}

// Write a value to a LTC2943 16 bit register (a pair of two 8 bit registers, MSB first). 
// Returns true if ok.
bool writeReg16(uint8 base_reg,  uint16 value) {
  bool error = false;
  error |= i2c::start(kLtc2943AddressByte | i2c::direction::WRITE);   
  error |= i2c::writeByte(base_reg);                       
  error |= i2c::writeByte(highByte(value));   
  error |= i2c::writeByte(lowByte(value));                        
  i2c::stop(); 
  return !error;  
}

// Read a LTC2943 8 bit register. Returns true if ok.
bool readReg8(uint8 reg, uint8* value) {
  bool error = false;
  error |= i2c::start(kLtc2943AddressByte | i2c::direction::WRITE);   
  error |= i2c::writeByte(reg);   
  error |= i2c::start(kLtc2943AddressByte | i2c::direction::READ); 
  // TODO: add to read ack/nack returned error status.
  *value = i2c::readByteWithNak();
  i2c::stop();
  return !error;
}

// Read a LTC2943 16 bit register (a pair of two 8 bit registers, MSB first). 
// Returns true if ok.
bool readReg16(uint8 reg, uint16* value) {
  bool error = false;
  error |= i2c::start(kLtc2943AddressByte | i2c::direction::WRITE);   
  error |= i2c::writeByte(reg);   
  error |= i2c::start(kLtc2943AddressByte | i2c::direction::READ);
  // TODO: add to read ack/nack returned error status.
  uint8 msb = i2c::readByteWithAck();
  uint8 lsb = i2c::readByteWithNak();
  i2c::stop();
  *value = word(msb, lsb);
  return !error;
}
  
}  // namespace ltc2943



