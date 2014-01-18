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

#include "hardware_clock.h"

#include "avr_util.h"
#include "passive_timer.h"

namespace action_buzzer {
  static const uint16 kFrequency = 4000;

  // Divider for given frequency, assuming 16Mhz clock, X256 prescaler.
  static const uint8 kDivider = (16000000L / 256) / kFrequency;

  // Output is OC0B from timer 0 (same as PD5).
  static const uint8 kPinMask =  H(PIND5);

  // Even index slots are ON, odd index slots are off. Values are
  // slot time in millis
  static const  uint16 kSlotTimesMillis[] PROGMEM = {
    50, // on
    200,
    50, // on
    1500,  
  };

  static const uint8 kNumSlots = ARRAY_SIZE(kSlotTimesMillis);

  // Action buzzer variables.
  static PassiveTimer timer;
  static boolean pending_actions;

  // Valid states as an uint8 enum.
  namespace states {
    // No pending actions. LED can be turned on as soon as a new action arrives.
    static const uint8 IDLE = 1;
    // Buzzer cycle is in the active portion.
    static const uint8 ACTIVE = 2;
  }

  // 0 Menas idle. Value > 0 indicates the steps of a buzzer cycle.
  static uint8 state;

  // When state is active, indicates the index of the current slot
  static uint8 active_slot_index;

  // Turn on.
  void on() {
    // Clear timer so we will get a nice first pules.
    TCNT0 = 0;
    // Enabled timer output on PD5.
    TCCR0A |=  H(COM0B1) | H(COM0B0) ;
    // Make sure PD5 is output.
    DDRD |= kPinMask;  
  }

  // Turn off.
  void off() {
    // Disable timer output
    TCCR0A &=  ~(H(COM0B1) | H(COM0B0)); 
    // Make sure PD5 is output and force low.
    DDRD |= kPinMask; 
    PORTD &= ~kPinMask;  
  }

  inline void enterIdleState() {
    state = states::IDLE;
    off();
  }

  inline void enterActiveState() {
    state = states::ACTIVE;
    active_slot_index = 0;
    timer.restart();
    on();
  } 

  void setup() {
    // Fast PWM mode, OC2B output active high.
    TCCR0A = L(COM0A1) | L(COM0A0) | H(COM0B1) | H(COM0B0) | H(WGM01) | H(WGM00); 
    // Prescaler x256 (62.5Khz @ 16Mhz).
    TCCR0B = L(FOC0A) | L(FOC0B) | H(WGM02) | H(CS02) | L(CS01) | L(CS00);
    // Clear counter.
    TCNT0 = 0;
    // Compare A, sets output frequency.
    OCR0A = kDivider;
    // Compare B, sets output duty cycle.
    OCR0B = kDivider / 2;
    // Diabled interrupts.
    TIMSK0 = L(OCIE0B) | L(OCIE0A) | L(TOIE0);
    TIFR0 =  L(OCF0B) | L(OCF0A) | L(TOV0); 

    enterIdleState();
  }

  // Called periodically from main loop() to do the state transitions. 
  void loop() {
    switch (state) {
      case states::IDLE:
      if (pending_actions) {
        enterActiveState();
        pending_actions = false;
      }
      break;

      case states::ACTIVE:
      // Test if we are done with this slot's time.
      const uint16 slot_time_millis = pgm_read_word(&kSlotTimesMillis[active_slot_index]);
      if (timer.timeMillis() > slot_time_millis) {
        timer.restart();
        active_slot_index++;
        // If done with last slot, enter idle state.
        if (active_slot_index >= kNumSlots) {
          enterIdleState();
        }
        // Move to next slot.
        else {
          // Odd slots are off, even are on.
          if (active_slot_index & 0x1) {
            off();    
          } 
          else {
            on();
          }
        }
      }
      break;  
    }
  }

  void action(boolean flag) {
    pending_actions = flag;  
  }

}  // namespace hardware_clock


