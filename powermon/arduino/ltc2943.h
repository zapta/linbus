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

#ifndef LTC2943_H
#define LTC2943_H

#include "avr_util.h"

// Linear Technology LTC2942 Fuel Guage IC driver.
namespace ltc2943 {
  
namespace regs8 {
  const uint8 kStatus = 0x00;
  const uint8 kControl = 0x01;
}

namespace regs16 {
  const uint8 kAccumCharge = 0x02;
  const uint8 kVoltage = 0x08;
  const uint8 kCurrent = 0x0e;
  const uint8 ktemperature = 0x14;
}

// Called once from main setup(). Also setup I2C.
extern void setup();

// Called once after setup and possibly more after an error. Reinitialize the device
// as needed. Returns true if ok.
extern bool init();

// TODO: make these read/write functions private and expose high level functions
// like readChargeRegiater(), readVoltageRegister(), readCurrentRegister();

// Write a value to a LTC2943 8 bit register. Returns true if ok.
extern bool writeReg8(uint8 reg8,  uint8 value);

// Write a value to a LTC2943 16 bit register. Returns true if ok.
extern bool writeReg16(uint8 reg16,  uint16 value);

// Read a LTC2943 8 bit register. Returns true if ok.
extern bool readReg8(uint8 reg8, uint8* value);

// Read a LTC2943 16 bit register. Returns true if ok.
extern bool readReg16(uint8 reg16, uint16* value); 
  
}  // namespace ltc2943

#endif  



