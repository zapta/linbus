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

#ifndef CLOCK_H
#define CLOCK_H

#include <arduino.h>

// Provides millisecond resolution time. Uses Timer1
// and polling, no interrupts.
namespace clock {
  extern void init();
  
  // Updates internal time and returns current time in millis
  // since init(). Should be called at intervals <= 230ms to avoid
  // missing timer overflow.
  extern uint32_t update_millis();
  
  // Provides access to the 16 bit hardware timer for finer granularity
  // timing. Indepdenent of update_millis(). Can be called from interrupt
  // routines.
  inline uint16_t hardware_ticks_mod_16_bit() {
    return TCNT1;
  }
  
  // 16Mhz / x64 prescaler.
  const uint32_t kHardwareTicksPerSecond = 250000;
}

#endif  

