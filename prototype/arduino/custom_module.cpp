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

// Like all the other custom_* files, thsi file should be adapted to the specific application. 
// The example provided is for a Sport Mode button press injector for 981/Cayman.
namespace custom_module {
  
// A single byte enum representing the states.
namespace states {
  static const uint8 INITIAL = 0;
  static const uint8 IGNITION_ON = 1;
  static const uint8 BUTTON_IS_PRESSED = 2;
  static const uint8 TOGGLE_CONFIG = 3;
  static const uint8 MAYBE_INJECT = 4;
  static const uint8 INJECT = 5;
  static const uint8 STABLE = 6;
}

// The current state. One of states:: values. 
static uint8 state;

// Tracks since change to current state.
static PassiveTimer time_in_state;
  
// STATUS LED - indicates when button is pressed (including injected presses).
static io_pins::OutputPin status_led(PORTD, 7);

// Tracks the ingition-on signal of the car.
static SignalTracker ignition_on_signal_tracker(3, 1000, 2000);

static SignalTracker sport_mode_button_signal_tracker(3, 1000, 2000);

// Injection trigger push button, 25ms debouncing, active low.
// For debugging.
// @@@ TODO: remove.
static IoButton debug_button(PORTB, 2, 25, false);

static inline boolean isButtonOn() {
  return sport_mode_button_signal_tracker.isOn() || debug_button.isPressed();
}

// Arbitrary 16bit code to store in the eeprom for on/off state.
namespace eeprom_uint16_code {
  static const uint16 ENABLED = 0x1234;
  static const uint16 DISABLED = 0x4568;
}

// Is this feature enabled. This value is persisted in the eeprom and can be
// toggled using using a long press on the Sport Mode button while turning the
// ignition on.
static boolean is_enabled;

// Set is_enabled from the configuration in the eeprom.
static inline void loadEepromConfig() {
  const uint16 eeprom_code = eeprom_read_word(0);
  // If the code is unknown we default to enabled.
  is_enabled = eeprom_code != eeprom_uint16_code::DISABLED;
  sio::printf(F("X config: %d\n"), is_enabled);
}

// Toggle the current configuration, with eeprom persistnce.
static inline void toggleConfig() {
  // Toggle the eeprom code.
  const uint16 eeprom_code = (is_enabled) ? eeprom_uint16_code::DISABLED : eeprom_uint16_code::ENABLED;
  eeprom_write_word(0, eeprom_code);

  // TODO: blinks the error LED if writing to the eeprom failed.

  // Read the new eeprom code. If writing to the eeprom failed, we
  // will stay with the actual config stored in the eeprom.
  loadEepromConfig();
}

static inline void changeToState(uint8 new_state) {
  state = new_state;
  sio::printf(F("X state: %d\n"), state);
  // We assume this is a new state and always reset the time in state.
  time_in_state.restart();
  // Make sure we do not inject outside of INJECTING state.
  if (state != states::INJECT)  {
    custom_injector::setInjectionsEnabled(false);
  }
}

void setup() {
  loadEepromConfig();
  changeToState(states::INITIAL);
}
  
// Called periodically from loop() to update the state machine.
static inline void updateState() {
  // Enforce safety rule. Injecting only in INJECT state.
  if (state != states::INJECT) {
    custom_injector::setInjectionsEnabled(false);
  }
   
  // General rule: if ignition is known to be off, reset to INITIAL state.
  if (ignition_on_signal_tracker.isOff()) {
    if (state != states::INITIAL) {
      changeToState(states::INITIAL);
    }  
    return;
  }
    
  // Handle the state transitions.
  switch (state) {
    case states::INITIAL:
      if (ignition_on_signal_tracker.isOnForAtLeastMillis(1000)) {
        changeToState(states::IGNITION_ON);  
      }
      break;
      
    case states::IGNITION_ON:
      changeToState(isButtonOn() ? states::BUTTON_IS_PRESSED : states::MAYBE_INJECT);  
      return;
    
    case states::BUTTON_IS_PRESSED:
      if (!isButtonOn()) {
        changeToState(states::MAYBE_INJECT);
        break;
      }
      if (time_in_state.timeMillis() >= 5000) {
        changeToState(states::TOGGLE_CONFIG);
      }
      break;
     
    case states::TOGGLE_CONFIG:
      toggleConfig();
      changeToState(states::MAYBE_INJECT);
      break;

    case states::MAYBE_INJECT:
      changeToState(is_enabled ? states::INJECT : states::STABLE);
      break;
    
    case states::INJECT:
      custom_injector::setInjectionsEnabled(true);
      if (time_in_state.timeMillis() > 500) {
        custom_injector::setInjectionsEnabled(false);
        changeToState(states::STABLE);
      }
      break;
    
    case states::STABLE:
      // Nothing to do here. Stay in this state until ignition is
      // turned off.
      break;
    
    // Unknown state, set to initial.
    default:
      sio::printf(F("state: unknown %d"), state);
      changeToState(states::INITIAL);
      break;
  } 
}
  
void loop() {
  // Update dependents.
  debug_button.loop();
  ignition_on_signal_tracker.loop();
  sport_mode_button_signal_tracker.loop();
  
  // Update status LED
  status_led.set(sport_mode_button_signal_tracker.isOn());
  
  // Update the state machine
  updateState();
}


// Handling of frame from sport mode button unit.
// Test frame  8e 00 04 00 00 00 00 00 00 <checksum>.
void frameArrived(const LinFrame& frame) {
  // We handle only framed of id 8e
  const uint8 id = frame.get_byte(0);
  
  // The sport mode button report frame. We make sure this is an original, non
  // injected one and that it has the expected size.
  if (id == 0x8e) {
    if (!frame.hasInjectedBits() && frame.num_bytes() == (1 + 8 + 1)) {
      const boolean button_is_pressed = frame.get_byte(2) & H(2);
      sport_mode_button_signal_tracker.reportSignal(button_is_pressed);
    }
     return;
  }
  
  if (id == 0x0d) {
    if (frame.num_bytes() == (1 + 8 + 1)) {
      const boolean is_ignition_bit_on = frame.get_byte(6) & H(7);
      ignition_on_signal_tracker.reportSignal(is_ignition_bit_on);
    }
    return;
  }
}

}  // namespace custom_module


