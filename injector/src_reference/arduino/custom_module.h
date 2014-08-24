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

#ifndef CUSTOM_MODULE_H
#define CUSTOM_MODULE_H

#include "avr_util.h"
#include "lin_frame.h"

// Implement the application specific functionality.
//
// Like all the other custom_* files, this file should be adapted to the specific application. 
// The example provided is for a Sport Mode button press injector for 981/Cayman.
namespace custom_module {
  // Called once during initialization.
  extern void setup();

  // Called once on each iteration of the Arduino main loop().
  extern void loop();

  // Called once when a new valid frame was recieved.
  extern void frameArrived(const LinFrame& frame);
  
}  // namespace custom_module

#endif

