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

#ifndef CUSTOM_CONFIG_H
#define CUSTOM_CONFIG_H

#include "avr_util.h"
#include "lin_frame.h"

// Implement the application specific configuration control. This is a single
// bit that is stored in the eeprom and enables/disable the injection. The configuration
// bit can be toggled by the user using application specific button signals intercepted
// from the lin frames.
//
// Like all the other custom_* files, thsi file should be adapted to the specific application. 
// The example provided is for a Sport Mode button press injector for 981/Cayman.
namespace custom_config {
  namespace private_ {
    // True when sport mode injection is enabled.
    extern boolean is_enabled;
  }

  // Called once during initialization.
  extern void setup();

  // Called once on each iteration of the Arduino main loop().
  extern void loop();

  // Called once when a new valid frame was recieved. Used to intercept
  // signals of buttons that affects the config.
  extern void frameArrived(const LinFrame& frame);
  
  inline boolean is_enabled() {
    return private_::is_enabled;
  }

}  // namespace custom_config

#endif

