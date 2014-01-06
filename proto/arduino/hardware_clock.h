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

#ifndef HARDWARE_CLOCK_H
#define HARDWARE_CLOCK_H

#include <arduino.h>
#include "avr_util.h"

// Provides a free running 16 bit counter with 250 ticks per millisecond and 
// about 280 millis cycle time. Assuming 16Mhz clock.
//
// USES: timer 1, no interrupts.
namespace hardware_clock {
  // Call once from main setup(). Tick count start at 0.
  extern void setup();

  // Free running 16 bit counter. Starts counting from zero and wraps around
  // every ~280ms. Ok to read from an ISR.
  inline uint16 ticks() {
    return TCNT1;
  }

  // @ 16Mhz / x64 prescaler. Number of ticks per a millisecond.
  const uint32 kTicksPerMilli = 250;
}  // namespace hardware_clock

#endif  


