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

#ifndef LED_H
#define LED_H

#include <arduino.h>

// Provides fast LED manipulation functions. Faster than
// Arduino's digitalWrite(...).
namespace leds {
  namespace internal {
    // On board LED. PB5, Arduino pin 13.
    const byte kLedMask = H(PINB5);

    // Auxilary LEDs.
    const byte kLed1Mask = H(PIND6); 
    const byte kLed2Mask = H(PIND7);
    const byte kLed3Mask = H(PINB0);
    const byte kLed4Mask = H(PINB1);
  }

  inline void on() {
    PORTB |= internal::kLedMask;
  }

  inline void off() {
    PORTB &= ~internal::kLedMask;
  }

  inline void on1() {
    PORTD |= internal::kLed1Mask;
  }

  inline void off1() {
    PORTD &= ~internal::kLed1Mask;
  }

  inline void on2() {
    PORTD |= internal::kLed2Mask;
  }

  inline void off2() {
    PORTD &= ~internal::kLed2Mask;
  }

  inline void on3() {
    PORTB |= internal::kLed3Mask;
  }

  inline void off3() {
    PORTB &= ~internal::kLed3Mask;
  }

  inline void on4() {
    PORTB |= internal::kLed4Mask;
  }

  inline void off4() {
    PORTB &= ~internal::kLed4Mask;
  }

  inline void init() {
    DDRB |= (internal::kLedMask | internal::kLed3Mask | internal::kLed4Mask);
    DDRD |= (internal::kLed1Mask | internal::kLed2Mask);

    off();
    off1();
    off2();
    off3();
    off4();
  }
}

#endif

