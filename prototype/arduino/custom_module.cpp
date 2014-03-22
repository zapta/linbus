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

#include "custom_config.h"
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
  static const uint8 INJECT = 2;
  static const uint8 STABLE = 3;
}

// The current state. One of states:: values. 
static uint8 state;

// Tracks since change to current state.
static PassiveTimer time_in_state;
  
// STATUS LED - indicates when button is pressed (including injected presses).
static io_pins::OutputPin status_led(PORTD, 7);

// Tracks the ingition-on signal of the car.
static SignalTracker ignition_on_signal_tracker(3, 1000, 2000);

static inline void changeToState(uint8 new_state) {
  state = new_state;
  sio::printf(F("injection state: %d\n"), state);
  // We assume this is a new state and always reset the time in state.
  time_in_state.restart();
}

void setup() {
  custom_config::setup();
  changeToState(states::INITIAL);
}
  
// Called periodically from loop() to update the state machine.
static inline void updateState() {
  // Enforce safety rule. Injecting only in INJECT state.
  const boolean injecting = state == states::INJECT;
  custom_injector::setInjectionsEnabled(injecting);
  status_led.set(injecting);
   
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
      changeToState(custom_config::is_enabled() ? states::INJECT : states::STABLE);  
      return;
    
    case states::INJECT:
      // NOTE: the meta rule at the begining of this method enables injection
      // as long as state is INJECT. We don't need to control injection here.
      if (time_in_state.timeMillis() > 500) {
        changeToState(states::STABLE);
      }
      break;
    
    case states::STABLE:
      // Nothing to do here. Stay in this state until ignition is
      // turned off.
      break;
    
    // Unknown state, set to initial.
    default:
      sio::printf(F("injection state: unknown (%d)"), state);
      changeToState(states::INITIAL);
      break;
  } 
}
  
void loop() {
  // Update dependents.
  custom_config::loop();
  ignition_on_signal_tracker.loop();
  
  // Update the state machine
  updateState();
}


// Handling of frame from sport mode button unit.
// Test frame  8e 00 04 00 00 00 00 00 00 <checksum>.
void frameArrived(const LinFrame& frame) {
  // Let the config manager tracks the frames it cares about.
  custom_config::frameArrived(frame);
  
  // Track the frames we care about in this file.
  const uint8 id = frame.get_byte(0);
  if (id == 0x0d) {
    if (frame.num_bytes() == (1 + 8 + 1)) {
      const boolean is_ignition_bit_on = frame.get_byte(6) & H(7);
      ignition_on_signal_tracker.reportSignal(is_ignition_bit_on);
    }
    return;
  }
}

}  // namespace custom_module


