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

#ifndef IO_PINS_H
#define IO_PINS_H

#include <arduino.h>

namespace io_pins {
  // A class to abstract an output pin that is not necesarily an arduino 
  // digital pin. Also optimized for fast setOn/Off.
  class OutputPin {
public:
    // port is either PORTB, PORTC, or PORTD.
    // bit index is one of [7, 6, 5, 4, 3, 2, 1, 0] (lsb).
    OutputPin(volatile uint8& port, uint8 bitIndex) :
      port_(port),
      bit_mask_(1 << bitIndex), 
      is_on_(false) {
        // NOTE: ddr port is is always one address below port.
        volatile uint8& ddr = *((&port)-1);
        ddr |= bit_mask_;
        port_ &= ~bit_mask_;
      } 

    inline void on() {
      is_on_ = true;
      port_ |= bit_mask_;
    }

    inline void off() {
      is_on_ = false;
      port_ &= ~bit_mask_;
    }

    void set(boolean v) {
      if (v) {
        on();
      } 
      else {
        off();
      }
    }

    inline boolean isOn() {
      return is_on_;
    }

    void toggle() {
      if (is_on_) {
        off();
      } 
      else {
        on();
      }
    }

private:
    volatile uint8& port_;
    const uint8 bit_mask_;
    boolean is_on_;
  };

  // A class to abstract an input pin that is not necesarily an arduino 
  // digital pin. Also optimized for quick access.
  class InputPin {
public:
    // port is either PORTB, PORTC, or PORTD.
    // bit index is one of [7, 6, 5, 4, 3, 2, 1, 0] (lsb).
    // NOTE: ddr port is is always one address below portx.
    // NOTE: pin port is is always two addresses below portx.
    InputPin(volatile uint8& port, uint8 bitIndex) 
: 
      pin_(*((&port)-2)),
      bit_mask_(1 << bitIndex) { 
        volatile uint8& ddr = *((&port)-1);
        ddr &= ~bit_mask_;  // input
        port |= bit_mask_;  // pullup
      } 

    inline boolean isHigh() {
      return pin_ & bit_mask_;
    }

private:
    volatile uint8& pin_;
    const uint8 bit_mask_;
  };

}  // namespace io_pins

#endif




