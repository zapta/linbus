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

#include "action_buzzer.h"

#include "avr_util.h"
#include "passive_timer.h"

namespace action_buzzer {
  // 2400Hz is the resonance frequency of Soberton WT-1205.
  static const uint16 kFrequency = 2400;

  // Divider for given frequency, assuming 16Mhz clock, X256 prescaler.
  static const uint8 kDivider = (16000000L / 256) / kFrequency;

  // Output is OC0B from timer 0 (same as PD5).
  static const uint8 kPinMask =  H(PIND5);

  // Even index slots are ON, odd index slots are off. Values are
  // slot time in millis
  static const  uint16 kSlotTimesMillis1[] PROGMEM = {
    200, // on
    150,
    200, // on
    150,
    200, // on
    150,
    200, // on
    150,
    200, // on
    3000, 
    0,   // end 
  };
  
  // Even index slots are ON, odd index slots are off. Values are
  // slot time in millis
  static const  uint16 kSlotTimesMillis2[] PROGMEM = {
    60, // on
    150,
    60, // on
    3000, 
    0,   // end 
  };
  
  static inline uint16 slotTimeMillis(uint8 sequence, uint8 slot_index) {
    if (sequence == 1) {
      return pgm_read_word(&kSlotTimesMillis1[slot_index]);
    }
    return pgm_read_word(&kSlotTimesMillis2[slot_index]);
  }

  // Action buzzer variables.
  static PassiveTimer timer;
  static boolean pending_actions;

  // State of the sequence playing logic. Can be active (true) or idle (false).
  static boolean state_is_active;

  // When in active state, indicates the current sequence we are playing (1 or 2).
  static uint8 active_sequence_number;
  
  // When state is active, indicates the index of the current slot
  static uint8 active_slot_index;

  // Turn buzzer on.
  void buzzerOn() {
    // Clear timer so we will get a nice first pules.
    TCNT0 = 0;
    // Enabled timer output on PD5.
    TCCR0A |=  H(COM0B1) | H(COM0B0) ;
    // Make sure PD5 is output.
    DDRD |= kPinMask;  
  }

  // Turn buzzer off.
  void buzzerOff() {
    // Disable timer output
    TCCR0A &=  ~(H(COM0B1) | H(COM0B0)); 
    // Make sure PD5 is output and force low.
    DDRD |= kPinMask; 
    PORTD &= ~kPinMask;  
  }

  inline void enterIdleState() {
    state_is_active = false;
    buzzerOff();
  }

  inline void enterActiveState() {
    state_is_active = true;
    active_sequence_number = 1;
    active_slot_index = 0;
    timer.restart();
    buzzerOn();
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
  
  // Called from loop() when in IDLE state.
  static inline void loopInIdleState() {
    if (pending_actions) {
      enterActiveState();
     pending_actions = false;
    }  
  }
  
  // Called from loop() when in ACTIVE state.
  static inline void loopInActiveState() {
    // If within current slot time then do nothing.
    const uint16 slot_time_millis = slotTimeMillis(active_sequence_number, active_slot_index);
    if (timer.timeMillis() < slot_time_millis) {
      return;
    }
    
    // Advance to next slot.
    timer.restart();
    active_slot_index++;
    const uint16 next_slot_time_millis = slotTimeMillis(active_sequence_number, active_slot_index);
        
    // If this is a normal slot, start playing it.
    if (next_slot_time_millis) {
      // Odd slots are off, even are on.
      if (active_slot_index & 0x1) {
        buzzerOff();    
      } 
      else {
        buzzerOn();
      }  
      return;
    }
       
    // Here when we reached a terminator slot at the end of the sequence. 
    
    // If there are pending action, start a new sequence.
    if (pending_actions) {
      // Stay in active state and start another cycle.
      pending_actions = false;
      // We play sequence 1 once and then switch to sequence 2.
      active_sequence_number = 2;
      active_slot_index = 0;
      // First slot is always on (even index).
      buzzerOn();
      return;
    } 
      
    // Here when at end of sequence and no pending action. Switch
    // to off state.
    enterIdleState();
  }
 
  // Called periodically from main loop() to do the state transitions. 
  void loop() {
    if (state_is_active) {
      loopInActiveState();
    } else {
      loopInIdleState();
    }
  }

  void action(boolean flag) {
    pending_actions = flag;
    
    // If negative action, abort buzzer immedielty. 
    if (!flag) {
      enterIdleState();  
    }
  }

}  // namespace action_buzzer


