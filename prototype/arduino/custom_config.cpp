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

#include "custom_module.h"

#include <avr/eeprom.h>

#include "custom_injector.h"
#include "io_button.h"
#include "io_pins.h"
#include "signal_tracker.h"
#include "sio.h"

// Provide injection enable/disable configuration bit (persisted in eeprom). Also allows the
// user to toggle that bit using car's buttons' states intercepted from linbus frames.
// The toggle squence is as follows:
// 1. Press and hold button A (On P981/CS: the Spoiler Up/Down button).
// 2. Click button B 5 times (On P981/CS: the Sport Mode button).
// 3. Release button A.
//
// Like all the other custom_* files, thsi file should be adapted to the specific application. 
// The example provided is for a Sport Mode button press injector for 981/Cayman.
namespace custom_config {

// Variables used in .h file.
namespace private_ {
  boolean is_enabled;
}
  
// A single byte enum representing the states of the buttons sequence detector.
namespace states {
  static const uint8 IDLE = 0;
  static const uint8 BUTTON_A_PRESSED = 1;
  static const uint8 BUTTON_A_RELEASED = 2;
  static const uint8 TOGGLE_CONFIG = 3;
}

// Current button recognizer state. One of states values. 
static uint8 state;

// Arbitrary 16bit code to store in the eeprom for on/off state.
namespace eeprom_uint16_code {
  static const uint16 ENABLED = 0x1234;
  static const uint16 DISABLED = 0x4568;
}

// Tracks the state of the configuration button A based on intercepted frames.
static SignalTracker button_A_signal_tracker(3, 1000, 2000);

// Tracks the state of the configuration button B based on intercepted frames.
static SignalTracker button_B_signal_tracker(3, 1000, 2000);

// Set is_enabled flag from the configuration in the eeprom.
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
  
  //sio::printf(F("state E1\n"));
  //uint16 time_before = hardware_clock::ticksForNonIsr();
  eeprom_write_word(0, eeprom_code);
  //sio::printf(F("state E2\n"));

  sio::printf(F("config toggled\n"));

  // TODO: blinks the error LED if writing to the eeprom failed.

  // Read the new eeprom code. If writing to the eeprom failed, we
  // will stay with the actual config stored in the eeprom.
  loadEepromConfig();
}

static inline void changeToState(uint8 new_state) {
  state = new_state;
  sio::printf(F("X config state: %d\n"), state);
}

void setup() {
  loadEepromConfig();
  changeToState(states::IDLE);
}
  
// Called periodically from loop() to update the state machine.
static inline void updateState() {
  static uint8 button_B_count;
  static uint8 button_B_last_state;
    
  // Handle the state transitions.
  switch (state) {
    case states::IDLE:
      if (button_A_signal_tracker.isOn()) {
        button_B_count = 0;
        button_B_last_state = button_B_signal_tracker.state();
        changeToState(states::BUTTON_A_PRESSED);  
      }
      break;
      
    case states::BUTTON_A_PRESSED: {
      if (!button_A_signal_tracker.isOn()) {
        changeToState(states::BUTTON_A_RELEASED);  
        break;
      }
      const uint8 button_B_state = button_B_signal_tracker.state();
      if ((button_B_last_state == SignalTracker::States::OFF) && (button_B_state == SignalTracker::States::ON)) {
        button_B_count++;
        sio::printf(F("config state: ->%d\n"), button_B_count);
      }
      button_B_last_state = button_B_state;
    }
      break;
    
    case states::BUTTON_A_RELEASED:
      changeToState((button_B_count == 5) ? states::TOGGLE_CONFIG : states::IDLE);
      break;
     
    case states::TOGGLE_CONFIG:
      toggleConfig();
      changeToState(states::IDLE);
      break;

    // Unknown state, set to initial.
    default:
      sio::printf(F("X config state: unknown (%d)"), state);
      changeToState(states::IDLE);
      break;
  } 
}
  
// Called repeatidly from the main loop().
void loop() {
  button_A_signal_tracker.loop();
  button_B_signal_tracker.loop();
  
  // Update the state machine
  updateState();
}

// Handling of frame from sport mode button unit.
void frameArrived(const LinFrame& frame) {
  const uint8 id = frame.get_byte(0);
  
  // The sport mode button report frame. We make sure this is an original, non
  // injected one and that it has the expected size.
  if (id == 0x8e) {
    // @@@ TODO: set actual byte and bit indices.
    if (!frame.hasInjectedBits() && frame.num_bytes() == (1 + 8 + 1)) {
      // Track config button A
      const boolean button_A_is_pressed = frame.get_byte(2) & H(3);
      button_A_signal_tracker.reportSignal(button_A_is_pressed);
      
      // Track config button B
      const boolean button_B_is_pressed = frame.get_byte(2) & H(2);
      button_B_signal_tracker.reportSignal(button_B_is_pressed);
    }
    return;
  }
}

}  // namespace custom_module


