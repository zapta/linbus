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

#include "car_module.h"
#include "io_pins.h"

namespace car_module {
  
// STATUS LED - indicates when button is pressed (including injected presses).
static io_pins::OutputPin status_led(PORTD, 7);

  
void setup() {
}
  
void loop() {
}
  
// Handling of frame from sport mode button unit.
// Test frame  8e 00 04 00 00 00 00 00 00 <checksum>.
void frameArrived(const LinFrame& frame) {
  // We handle only framed of id 8e
  const uint8 id = frame.get_byte(0);
  if (id != 0x8e) {
    return;
  }
  
  // We expect a frame with one id byte, 8 data bytes and one checksum byte.
  if (frame.num_bytes() != (1 + 8 + 1)) {
    return;
  }
  
  const boolean button_pressed = frame.get_byte(2) & H(2);
  status_led.set(button_pressed);
}

}  // namespace car_module


