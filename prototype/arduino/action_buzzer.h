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

#ifndef BUZZER_H
#define BUZZER_H

#include <arduino.h>
#include "avr_util.h"

// Controlled the buzzer signal on OC0B/PD5 pin. Uses timer 0.
namespace action_buzzer {
  // Call once from main setup(). Buzzer starts in off state.
  extern void setup();

  // Call from each main loop().
  extern void loop();

  // Buzzer wil keep beeping as long as this is called with true.
  // Calling with false  cancels pending actions.
  extern void action(boolean flag);  
}  // namespace hardware_clock

#endif  



