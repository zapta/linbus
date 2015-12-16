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

#include "custom_config.h"

#include <avr/eeprom.h>

#include "custom_signals.h"
#include "passive_timer.h"
#include "sio.h"

// Like all the other custom_* files, this file should be adapted to the specific application. 
// The example provided is for a Sport Mode button press injector for 981/Cayman.
namespace custom_config {

  // The entire config toggle suquence from ignition on to ignition off must be completed within
  // time time.
  static const uint16 kSequenceTimeoutMillis = 20 * 1000;

  // The config toggle sequence need to include exactly this number of consecutive
  // button presses.
  static const uint8 kExpectedButtonClicks = 6;

  // Variables used in .h file.
  namespace private_ {
    boolean is_enabled;
  }

  // A single byte enum representing the states of the config sequence state machine.
  namespace states {
    static const uint8 IGNITION_OFF_IDLE = 0;
    static const uint8 IGNITION_ON_COUNTING = 1;
    static const uint8 IGNITION_ON_IDLE = 2;
    static const uint8 IGNITION_OFF_TOGGLE_CONFIG = 3;
  }

  // Current button recognizer state. One of states:: values. 
  static uint8 state;

  // Time in current state.
  static PassiveTimer time_in_state;

  // Arbitrary 16bit code to store in the eeprom for on/off state.
  namespace eeprom_uint16_code {
    static const uint16 ENABLED = 0x1234;
    static const uint16 DISABLED = 0x4568;
  }

  // Set is_enabled flag from the configuration stored in the eeprom.
  static inline void loadEepromConfig() {
    const uint16 eeprom_code = eeprom_read_word(0);

    // If the code is unknown we default to enabled.
    private_::is_enabled = eeprom_code != eeprom_uint16_code::DISABLED;
    sio::printf(F("config loaded: %d\n"), private_::is_enabled);
  }

  // Toggle the current configuration, with eeprom persistnce.
  static inline void toggleConfig() {
    // Toggle the eeprom code.
    const uint16 eeprom_code = (private_::is_enabled) ? eeprom_uint16_code::DISABLED : eeprom_uint16_code::ENABLED;
    eeprom_write_word(0, eeprom_code);
    sio::printf(F("config toggled\n"));

    // TODO: if the writing failed surface an error condition.

    // Read the new eeprom code. If writing to the eeprom failed, we
    // will stay with the actual config stored in the eeprom.
    loadEepromConfig();
  }

  // Change to given state. Assumes not already in this state.
  static inline void changeToState(uint8 new_state) {
    state = new_state;
    sio::printf(F("config state: %d\n"), state);
    time_in_state.restart();
  }

  void setup() {
    loadEepromConfig();
    changeToState(states::IGNITION_OFF_IDLE);
  }

  // Called periodically from loop() to update the state machine.
  static inline void updateState() {
    // Valid in IGNITION_ON_COUNTING state only.
    static uint8 button_click_count;
    static uint8 button_last_state;

    // Handle the state transitions.
    switch (state) {
      case states::IGNITION_OFF_IDLE:
        if (custom_signals::ignition_state().isOn()) {
          button_click_count = 0;
          button_last_state = custom_signals::config_button().state();
          changeToState(states::IGNITION_ON_COUNTING);  
        }
        break;

      case states::IGNITION_ON_COUNTING: 
        {
          // If sequence takes too long too long or too many clicks then ignore.
          if (time_in_state.timeMillis() > kSequenceTimeoutMillis || button_click_count > kExpectedButtonClicks) {
            changeToState(states::IGNITION_ON_IDLE);  
            break;
          }
  
          // If ignition turned off, see if we have the conditions to toggle configuration.
          if (custom_signals::ignition_state().isOff()) {
            changeToState(button_click_count == kExpectedButtonClicks 
                ? states::IGNITION_OFF_TOGGLE_CONFIG 
                : states::IGNITION_OFF_IDLE);
            break;
          }
  
          const uint8 button_new_state = custom_signals::config_button().state();
          // Count change from non pressed to pressed.
          if ((button_last_state == SignalTracker::States::OFF) && (button_new_state == SignalTracker::States::ON)) {
            // This cannot overflow because we exist this state if exceeding kExpectedButtonClicks. 
            button_click_count++;
            sio::printf(F("config state: %d.%d\n"), states::IGNITION_ON_COUNTING, button_click_count);
          }
          button_last_state = button_new_state;
        }
        break;

      case states::IGNITION_ON_IDLE:
        if (custom_signals::ignition_state().isOff()) {
          changeToState(states::IGNITION_OFF_IDLE);  
        }
        break;

      case states::IGNITION_OFF_TOGGLE_CONFIG:
        toggleConfig();
        changeToState(states::IGNITION_OFF_IDLE);
        break;

      // Unknown state, set to initial.
      default:
        sio::printf(F("config state: unknown (%d)"), state);
        // Go to a default state and wait there until ignition is off.
        changeToState(states::IGNITION_ON_IDLE);
        break;
    } 
  }

  // Called repeatidly from the main loop().
  void loop() {
    // Update the state machine.
    updateState();
  }

}  // namespace custom_module

