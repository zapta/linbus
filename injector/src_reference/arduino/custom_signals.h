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
  
}  // namespace custom_signals

#endif

