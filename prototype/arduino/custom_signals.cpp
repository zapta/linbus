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

#include "custom_signals.h"

#include "signal_tracker.h"

// Like all the other custom_* files, thsi file should be adapted to the specific application. 
// The example provided is for a Sport Mode button press injector for 981/Cayman.
namespace custom_signals {

// Variables used in .h file.
namespace private_ {
  SignalTracker ignition_on_signal_tracker(3, 1000, 2000);
  SignalTracker button_signal_tracker(3, 1000, 2000);
}

void setup() {
  // Nothing to do here.
}
  
// Called repeatidly from the main loop().
void loop() {
  // Loop dependents.
  private_::ignition_on_signal_tracker.loop();
  private_::button_signal_tracker.loop();
}

// Handling of frame from sport mode button unit.
void frameArrived(const LinFrame& frame) {
  // We ignore injected frames, not to be influence by own injections.
  if (frame.hasInjectedBits()) {
    return;
  }

  // Get frame id.
  const uint8 id = frame.get_byte(0);
  
  // Handle the frame with config button status bit.
  if (id == 0x8e) {
    if (frame.num_bytes() == (1 + 8 + 1)) {
      const boolean button_is_pressed = frame.get_byte(2) & H(2);
      private_::button_signal_tracker.reportSignal(button_is_pressed);
    }
    return;
  }

  // Handle the frame with ignition state status bit.
  if (id == 0x0d) {
    if (frame.num_bytes() == (1 + 8 + 1)) {
      const boolean is_ignition_bit_on = frame.get_byte(6) & H(7);
      private_::ignition_on_signal_tracker.reportSignal(is_ignition_bit_on);
    }
    return;
  }
}

}  // namespace custom_signals


