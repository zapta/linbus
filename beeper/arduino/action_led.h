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

#ifndef ACTION_LED_H
#define ACTION_LED_H

#include <arduino.h>
#include "io_pins.h"
#include "passive_timer.h"

// Wrapes an OutputPin with logic to blick an LED while some events occur. Design
// to be visible regardless of the event frequency and duration.
// Requires loop() calls from main loop().
class ActionLed {
public:
  ActionLed(volatile uint8& port, uint8 bitIndex) 
    : led_(port, bitIndex),
      pending_actions_(false) {
    enterIdleState();
  }
  
  // Called periodically from main loop() to do the state transitions. 
  void loop() {
    switch (state_) {
      case kState_IDLE:
      if (pending_actions_) {
        enterActiveOnState();
        pending_actions_ = false;
      }
      break;

      case kState_ACTIVE_ON:
        if (timer_.timeMillis() > 20) {
          enterActiveOffState();
        }
      break;
      case kState_ACTIVE_OFF:
      if (timer_.timeMillis() > 30) {
          // NOTE: if pending_actions_ then will enter ACTIVE_ON on next iteration.
          enterIdleState();
        }
      break;  
    }
  }
  
  void action() {
    pending_actions_ = true;  
  }
  
private:
    // No pending actions. LED can be turned on as soon as a new action arrives.
    static const uint8 kState_IDLE = 1;
    // LED is pulsed on.
    static const uint8 kState_ACTIVE_ON = 2;
    // LED was pulsed on and is now in a blackup period until it can be turned
    // on again.
   static const uint8 kState_ACTIVE_OFF = 3; 
   uint8 state_; 
  
  // The underlying pin of the led. Active high.
  io_pins::OutputPin led_;
  
  // A timer for the ACtIVE_ON and ACTIVE_OFF periods.
  PassiveTimer timer_;
  
  // Indicates if a new action arrived.
  boolean pending_actions_;
  
  inline void enterIdleState() {
    state_ = kState_IDLE;
    led_.low();
  }
  
  inline void enterActiveOnState() {
    state_ = kState_ACTIVE_ON;
    led_.high();
    timer_.restart();
    
  }
  
  inline void enterActiveOffState() {
    state_ = kState_ACTIVE_OFF;
    led_.low();
    timer_.restart();
  }
};

#endif  


