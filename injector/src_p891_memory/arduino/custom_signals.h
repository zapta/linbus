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

#ifndef CUSTOM_SIGNALS_H
#define CUSTOM_SIGNALS_H

#include "avr_util.h"
#include "lin_frame.h"
#include "signal_tracker.h"

// Tracks signals on the linbus that we use for this custom application.
//
// Like all the other custom_* files, this file should be adapted to the specific application. 
// The example provided is for a Sport Mode button press injector for 981/Cayman.
namespace custom_signals {
  namespace private_ {
    // Tracks the ingition-on status.
    extern SignalTracker ignition_on_signal_tracker;
    
    // Tracks the state of the config button.
    // This button is mapped to the P981/CS Sport Mode button.
    extern SignalTracker button_signal_tracker;

    // 
    // 981 buttons and LED indicators
    //

    extern SignalTracker autostart_switch;
    extern SignalTracker autostart_LED;

    extern SignalTracker PASM_switch;
    extern SignalTracker PASM_LED;

    extern SignalTracker PSE_switch;
    extern SignalTracker PSE_LED;

    extern SignalTracker PSM_switch;
    extern SignalTracker PSM_LED;

    extern SignalTracker roof_close_switch;
    extern SignalTracker roof_open_switch;

    extern SignalTracker spoiler_switch;
    extern SignalTracker spoiler_LED;

    extern SignalTracker sport_switch;
    extern SignalTracker sport_LED;

    extern SignalTracker sport_plus_switch;
    extern SignalTracker sport_plus_LED;
  }

  // Called once during initialization.
  extern void setup();

  // Called once on each iteration of the Arduino main loop().
  extern void loop();

  // Called once when a new valid frame was recieved. Used to intercept
  // signals of buttons that affects the config.
  extern void frameArrived(const LinFrame& frame);

  inline const SignalTracker& ignition_state() {
    return private_::ignition_on_signal_tracker;
  }

  inline const SignalTracker& config_button() {
    return private_::button_signal_tracker;
  }

  // 981 button/LED accessors
  inline const SignalTracker& autostart_switch() {
    return private_::autostart_switch;
  }

  inline const SignalTracker& autostart_LED() {
    return private_::autostart_LED;
  }

  inline const SignalTracker& PASM_switch() {
    return private_::PASM_switch;
  }

  inline const SignalTracker& PASM_LED() {
    return private_::PASM_LED;
  }

  inline const SignalTracker& PSE_switch() {
    return private_::PSE_switch;
  }

  inline const SignalTracker& PSE_LED() {
    return private_::PSE_LED;
  }

  inline const SignalTracker& PSM_switch() {
    return private_::PSM_switch;
  }

  inline const SignalTracker& PSM_LED() {
    return private_::PSM_LED;
  }

  inline const SignalTracker& roof_close_switch() {
    return private_::roof_close_switch;
  }

  inline const SignalTracker& roof_open_switch() {
    return private_::roof_open_switch;
  }

  inline const SignalTracker& spoiler_switch() {
    return private_::spoiler_switch;
  }

  inline const SignalTracker& spoiler_LED() {
    return private_::spoiler_LED;
  }

  inline const SignalTracker& sport_switch() {
    return private_::sport_switch;
  }

  inline const SignalTracker& sport_LED() {
    return private_::sport_LED;
  }

  inline const SignalTracker& sport_plus_switch() {
    return private_::sport_plus_switch;
  }

  inline const SignalTracker& sport_plus_LED() {
    return private_::sport_plus_LED;
  }
}  // namespace custom_signals

#endif

