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
#include "custom_signals.h"
#include "io_pins.h"
#include "signal_tracker.h"
#include "sio.h"

// Like all the other custom_* files, thsi file should be adapted to the specific application. 
// The example provided is for a Sport Mode button press injector for 981/Cayman.
namespace custom_module {
  
// A single byte enum representing the states.
namespace states {
  static const uint8 IGNITION_OFF_IDLE = 0;
  static const uint8 IGNITION_ON_MAYBE_INJECT = 1;
  static const uint8 IGNITION_ON_INJECT = 2;
  static const uint8 IGNITION_ON_IDLE = 3;
}

// The current state. One of states:: values. 
static uint8 state;

// Tracks since change to current state.
static PassiveTimer time_in_state;
  
// STATUS LED - indicates when button is pressed (including injected presses).
static io_pins::OutputPin status_led(PORTD, 7);

static inline void changeToState(uint8 new_state) {
  state = new_state;
  sio::printf(F("injection state: %d\n"), state);
  // We assume this is a new state and always reset the time in state.
  time_in_state.restart();
}

void setup() {
  custom_signals::setup();
  custom_config::setup();
  changeToState(states::IGNITION_OFF_IDLE);
}
  
// Called periodically from loop() to update the state machine.
static inline void updateState() {
  // Meta rule: inject IFF in INJECT state.
  {
    const boolean injecting = (state == states::IGNITION_ON_INJECT);
    custom_injector::setInjectionsEnabled(injecting);
    status_led.set(injecting);
  }
   
  // Meta rule: if ignition is known to be off, reset to IGNITION_OFF_IDLE state.
  if (custom_signals::ignition_state().isOff()) {
    if (state != states::IGNITION_OFF_IDLE) {
      changeToState(states::IGNITION_OFF_IDLE);
    }  
    return;
  }
    
  // Handle the state transitions.
  switch (state) {
    case states::IGNITION_OFF_IDLE:
      if (custom_signals::ignition_state().isOnForAtLeastMillis(1000)) {
        changeToState(states::IGNITION_ON_MAYBE_INJECT);  
      }
      break;
      
    case states::IGNITION_ON_MAYBE_INJECT:
      changeToState(custom_config::is_enabled() 
          ? states::IGNITION_ON_INJECT 
          : states::IGNITION_ON_IDLE);  
      return;
    
    case states::IGNITION_ON_INJECT:
      // NOTE: the meta rule at the begining of this method enables injection
      // as long as state is INJECT. We don't need to control injection here.
      if (time_in_state.timeMillis() > 500) {
        changeToState(states::IGNITION_ON_IDLE);
      }
      break;
    
    case states::IGNITION_ON_IDLE:
      // Nothing to do here. Stay in this state until ignition is
      // turned off.
      break;
    
    // Unknown state, set to initial.
    default:
      sio::printf(F("injection state: unknown (%d)"), state);
      changeToState(states::IGNITION_OFF_IDLE);
      break;
  } 
}
  
void loop() {
  // Update dependents.
  custom_signals::loop();
  custom_config::loop();
  
  // Update the state machine
  updateState();
}

void frameArrived(const LinFrame& frame) {
  // Track the signals in this frame.
  custom_signals::frameArrived(frame);
}

}  // namespace custom_module


