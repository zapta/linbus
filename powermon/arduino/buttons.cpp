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

#include "buttons.h"

namespace buttons {
  
namespace private_ {
  // TODO: make this a const.
  // Debounce time = 50ms.
  ByteDebouncer byte_debouncer(50);
  io_pins::InputPin action_button(PORTD, 2);
}

void loop() {
  // NOTE: since we debounce the entrie byte we must used unique values for true and false.
  // NOTE: the button is active low so we invert the signal.
  const uint8 new_value = private_::action_button.isHigh() ? 0 : 1; 
  private_::byte_debouncer.update(new_value);
}

}  // namepsace buttons



