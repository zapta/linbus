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

#include "config.h"

namespace config {
  
// The four configuration switches are connected to the 4 LSB pins of port C.
static const uint8 kBitMask = 0x0f;

namespace private_ {
  // TODO: make this a const.
  // Debounce time = 50ms.
  ByteDebouncer byte_debouncer(50);
}

void setup() {
  // Set the 4 config pins as inputs with weak pullup.
  DDRC &= ~kBitMask;  // input
  PORTC |= kBitMask;  // pullup
}

void loop() {
  // Read the 4 LSB pins, invert since they are active low.
  const uint8 new_value = (~PINC) & kBitMask; 
  private_::byte_debouncer.update(new_value);
}

}  // namepsace config



