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

#ifndef IO_BUTTON_H
#define IO_BUTTON_H

#include "avr_util.h"
#include "io_pins.h"
#include "debouncer.h"

// A class that wraps a button pin and a debouncer.
class IoButton {
 public:
  IoButton(volatile uint8& port, uint8 bitIndex, 
      int debounce_time_millis, boolean is_active_high)
   : 
    input_pin_(port, bitIndex),
    debouncer_(debounce_time_millis),
    is_active_high_(is_active_high) {
  }

  // Call from the main loop() method to update the debouncer.
  inline void loop() {
    const boolean isPressed = (input_pin_.isHigh() == is_active_high_);
    debouncer_.update(isPressed);
  }
 
  // Is button pressed? (post debouncing).
  inline boolean isPressed() const {
    return debouncer_.hasStableValue() && debouncer_.theStableValue();
  }

 private:
  io_pins::InputPin input_pin_;
  Debouncer debouncer_;
  const boolean is_active_high_;
};

#endif

