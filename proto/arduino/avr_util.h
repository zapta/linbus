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

#ifndef AVR_UTIL_H
#define AVR_UTIL_H

#include <arduino.h>

// Get rid of the _t type suffix.
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;

// Bit index to bit mask.
// AVR registers bit indices are defined in iom328p.h.
#define H(x) (1 << (x))
#define L(x) (0 << (x))

namespace avr_util {

  // Initialize timer0 to do nothing and be invisible,
  // no interrupts, etc.
  inline void timer0_off() {
    TCCR0A = 0;
    TCCR0B = 0;
    TCNT0 = 0;
    OCR0A = 0;
    OCR0B = 0;
    TIMSK0 = 0;
    TIFR0 = 0;
  } 

}  // namespace avr_util

#endif

