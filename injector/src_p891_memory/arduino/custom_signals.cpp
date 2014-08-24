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

// Like all the other custom_* files, this file should be adapted to the specific application. 
// The example provided is for a Sport/PSE button memory feature for 981/Cayman.
namespace custom_signals {

// Variables used in .h file.
namespace private_ {
  SignalTracker ignition_on_signal_tracker(3, 1000, 2000);
  // NOTE: we require only a single button report to change state. This prevents
  // missing clicks when clicking fast.
  SignalTracker button_signal_tracker(1, 1000, 2000);

  //
  // 981 buttons and LEDs
  //

  SignalTracker autostart_switch    (1, 1000, 2000); 
  SignalTracker autostart_LED       (1, 1000, 2000); 
                                    
  SignalTracker PASM_switch         (1, 1000, 2000); 
  SignalTracker PASM_LED            (1, 1000, 2000); 
                                    
  SignalTracker PSE_switch          (1, 1000, 2000);
  SignalTracker PSE_LED             (1, 1000, 2000);
                                    
  SignalTracker PSM_switch          (1, 1000, 2000); 
  SignalTracker PSM_LED             (1, 1000, 2000); 

  SignalTracker roof_close_switch   (1, 1000, 2000); 
  SignalTracker roof_open_switch    (1, 1000, 2000); 

  SignalTracker spoiler_switch      (1, 1000, 2000); 
  SignalTracker spoiler_LED         (1, 1000, 2000); 
                                    
  SignalTracker sport_switch        (1, 1000, 2000);
  SignalTracker sport_LED           (1, 1000, 2000);
                                    
  SignalTracker sport_plus_switch   (1, 1000, 2000); 
  SignalTracker sport_plus_LED      (1, 1000, 2000); 
}

void setup() {
  // Nothing to do here.
}
  
// Called repeatidly from the main loop().
void loop() {
  // Loop dependents.
  private_::ignition_on_signal_tracker.loop();
  private_::button_signal_tracker.loop();

  private_::autostart_switch.loop();
  private_::autostart_LED.loop();
  private_::PASM_switch.loop();
  private_::PASM_LED.loop();
  private_::PSE_switch.loop();
  private_::PSE_LED.loop();
  private_::PSM_switch.loop();
  private_::PSM_LED.loop();
  private_::roof_close_switch.loop();
  private_::roof_open_switch.loop();
  private_::spoiler_switch.loop();
  private_::spoiler_LED.loop();
  private_::sport_switch.loop();
  private_::sport_LED.loop();
  private_::sport_plus_switch.loop();
  private_::sport_plus_LED.loop();
}

// Handling of frame from sport mode button unit.
void frameArrived(const LinFrame& frame) {
  // We ignore injected frames, not to be influence by own injections.
  if (frame.hasInjectedBits()) {
    return;
  }

  // Get frame id.
  const uint8 id = frame.get_byte(0);
  
  // Handle slave-to-master frames (physical switches)
  if (id == 0x8e) {
    if (frame.num_bytes() == (1 + 8 + 1)) {
      const boolean button_is_pressed = frame.get_byte(2) & H(2);
      private_::button_signal_tracker.reportSignal(button_is_pressed);

      private_::autostart_switch.reportSignal   (frame.get_byte(4) & H(2));
      private_::PASM_switch.reportSignal        (frame.get_byte(1) & H(3));
      private_::PSE_switch.reportSignal         (frame.get_byte(2) & H(7));
      private_::PSM_switch.reportSignal         (frame.get_byte(3) & H(0));
      private_::roof_close_switch.reportSignal  (frame.get_byte(3) & H(7));
      private_::roof_open_switch.reportSignal   (frame.get_byte(3) & H(6));
      private_::spoiler_switch.reportSignal     (frame.get_byte(2) & H(3));
      private_::sport_switch.reportSignal       (frame.get_byte(2) & H(2));
      private_::sport_plus_switch.reportSignal  (frame.get_byte(2) & H(4));
    }
    return;
  }

  // Handle master-to-slave frames (ignition state, button LEDs...)
  if (id == 0x0d) {
    if (frame.num_bytes() == (1 + 8 + 1)) {
      const boolean is_ignition_bit_on = frame.get_byte(6) & H(7);
      private_::ignition_on_signal_tracker.reportSignal(is_ignition_bit_on);

      private_::autostart_LED.reportSignal      (frame.get_byte(4) & H(4));
      private_::PASM_LED.reportSignal           (frame.get_byte(1) & H(4));   // (both H(4) and H(5) observed on 2013 981BS USA model, neither are turned on by Sport Plus button...?)
      private_::PSE_LED.reportSignal            (frame.get_byte(3) & H(6));
      private_::PSM_LED.reportSignal            (frame.get_byte(4) & H(2));   // (both H(2) and H(7) observed on 2013 981BS USA model)
      private_::spoiler_LED.reportSignal        (frame.get_byte(3) & H(3));
      private_::sport_LED.reportSignal          (frame.get_byte(4) & H(0));
      private_::sport_plus_LED.reportSignal     (frame.get_byte(4) & H(5));
    }
    return;
  }
}

}  // namespace custom_signals


