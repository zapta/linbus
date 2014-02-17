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

#ifndef CAR_MODULE_CONFIG_H
#define CAR_MODULE_CONFIG_H

#include "avr_util.h"
#include "lin_frame.h"

// Tracks the config button linframes, and toggles the config bit when
// it is long pressed. The config bit enables/disables the buzzer controlled
// by car_module.cpp.
namespace car_module_config {
  // Called once during initialization.
  extern void setup();

  // Called once on each iteration of the Arduino main loop().
  inline void loop() {
    // Nothing to do here for now.
  }

  // Called once when a new valid frame was recieved.
  extern void frameArrived(const LinFrame& frame);

  // Is the car module feature currently enabled?
  extern boolean isEnabled();
  
  // Sets/reset the internal flag that allows/disallow config bit change
  // via button long presses.
  extern void allowConfigChanges(boolean allow);
  
}  // namespace car_module_config

#endif

