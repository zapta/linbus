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

#include "car_module_config.h"

#include <avr/eeprom.h>
#include "passive_timer.h"

// TODO: remove after initial debugging.
//#include "sio.h"

namespace car_module_config {

  // Arbitrary 16bit code to store in the eeprom for on/off state.
  namespace eeprom_uint16_code {
    static const uint16 ENABLED = 0x1234;
    static const uint16 DISABLED = 0x4568;
  }

  // States of the button tracker. Not using enum to avoid
  // 16 bit values.
  namespace button_tracker_states {
    const uint8 IDLE = 1;
    const uint8 NON_PRESSED = 2;
    const uint8 PRESSED_SHORT = 3;
    const uint8 PRESSED_LONG = 4;  
  }

  // One of button_tracker_states.
  static byte button_tracker_state;

  // When not in IDLE state, indicates how long the button has been pressed.
  static PassiveTimer button_press_timer;
  
  // Tracks the intervals between successive button reports. If too long
  // (e.g. missing frames) we reset the tracker
  static PassiveTimer button_interval_timer;
  
  // Contains the current enabled/disabled feature status.
  static boolean is_enabled;
  
  // When false, config bit changes via button long presses are disabled.
  static boolean allow_config_change;

  // Set is_enabled from the configuration in the eeprom.
  static void loadEepromConfig() {
    const uint16 eeprom_code = eeprom_read_word(0);
    // If the code is unknown we default to enabled.
    is_enabled = eeprom_code != eeprom_uint16_code::DISABLED; 
  }

  // Toggle the current configuration, with eeprom persistnce.
  static void toggleConfig() {
    // Toggle the eeprom code.
    const uint16 eeprom_code = (is_enabled) ? eeprom_uint16_code::DISABLED : eeprom_uint16_code::ENABLED;
    eeprom_write_word(0, eeprom_code); 
    
    // TODO: blinks the error LED if writing to the eeprom failed.

    // Read the new eeprom code. If writing to the eeprom failed, we
    // will stay with the actual config stored in the eeprom.
    loadEepromConfig();  
  }

  // Called whenever we get a report of button position.
  static void trackButtonState(boolean button_pressed) {
    // Track time from previous button state report.
    const uint32 button_report_interval =  button_interval_timer.timeMillis();
    button_interval_timer.restart();
    
    // If config change not allowed, no point of tracking the button.
    if (!allow_config_change) {
      button_tracker_state = button_tracker_states::IDLE;
      return;
    }

    // Handle the case of non pressed button.
    if (!button_pressed) {
      if (button_tracker_state != button_tracker_states::NON_PRESSED) {
        button_tracker_state = button_tracker_states::NON_PRESSED;
      }
      return;
    }
    
    // Here when button is pressed. 
    //
    // Handle the case of too long interval from previous button state report.
    if (button_report_interval > 2000) {
      button_tracker_state = button_tracker_states::IDLE;
      return;
    }
    
    // Handle the case of button pressed.
    switch (button_tracker_state) {
      case button_tracker_states::IDLE:
        // We stay in idle mode. We require at least one non-pressed button
        // report before allowing changes to transition to pressed.
        return;
        
      case button_tracker_states::NON_PRESSED:
        button_tracker_state = button_tracker_states::PRESSED_SHORT;
        // This starts the timing of detecting a long press.
        button_press_timer.restart();
        return;
        
      case button_tracker_states::PRESSED_SHORT:
        // Detect the long press. We requires 10 seconds.
        if (button_press_timer.timeMillis() >= 10000) {
          toggleConfig();
          button_tracker_state = button_tracker_states::PRESSED_LONG; 
        }
        return;
        
      case button_tracker_states::PRESSED_LONG:
        return;
    }
  }

  void frameArrived(const LinFrame& frame) {
    // We handle only framed of id 97.
    const uint8 id = frame.get_byte(0);
    if (id != 0x97) {
      return;
    }

    // We expect a frame with one ID byte, 5 data bytes and one checksum byte. 
    if (frame.num_bytes() != (1 + 5 + 1)) {
      return;
    }

    // Specific logic for P981/CS linbus of the homelink console. Indicates 
    // the state of the button in the upper center console near the windshield
    // mirror.
    // Test frame: 97 00 00 00 80 00 <checksum>.
    const boolean button_pressed = frame.get_byte(4) & H(7);
    trackButtonState(button_pressed);
  }

  boolean isEnabled() {
    return is_enabled;
  }

  void setup() {
    button_tracker_state = button_tracker_states::IDLE;
    loadEepromConfig();
    allow_config_change = false;
    button_interval_timer.restart();
  }
  
  void allowConfigChanges(boolean allow) {
    // Handle do nothing calls.
    if (allow == allow_config_change) {
      return;
    } 
    allow_config_change = allow;
  }
}  // namespace car_module_config



