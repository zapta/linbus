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

#include "lin_decoder.h"
#include "avr_util.h"
#include "leds.h"
#include "clock.h"

namespace lin_decoder {
  // TODO: these are for 9600. Change to 19200.
  static const uint8 kCountsPerTick = 208;
  // 9600 -> 26. 19200 -> 13.
  static const uint8 kClockTicksPerBit = clock::kHardwareTicksPerSecond / 9600;

  // Pins for communicating with the LIN transceiver.
  static const uint8 kRxPinMask  = H(PIND2);
  static const uint8 kTxPinMask = H(PINB2);
  static const uint8 kEnPinMask = H(PIND4);

  static void initLinPins() {
    // RX input, pull up.
    DDRD &= ~kRxPinMask;
    PORTD |= kRxPinMask;
    // TX output, default high.
    DDRB |= kTxPinMask;
    PORTB &= ~kTxPinMask; 
    // Enable output, default high.
    DDRD |= kEnPinMask;
    PORTD |= kEnPinMask;
  }

  static void initTimer() {    
    // OC2B cycle pulse (Arduino digital pin 3, PD3). For debugging.
    DDRD |= H(DDD3);
    // Fast PWM mode, OC2B output active high.
    TCCR2A = L(COM2A1) | L(COM2A0) | H(COM2B1) | H(COM2B0) | H(WGM21) | H(WGM20);
    // Prescaler: X8.
    TCCR2B = L(FOC2A) | L(FOC2B) | H(WGM22) | L(CS22) | H(CS21) | L(CS20);
    // Clear counter.
    TCNT2 = 0;
    // Determines baud rate.
    OCR2A = kCountsPerTick - 1;
    // A short 8 clocks pulse on OC2B at the end of each cycle,
    // just before triggering the ISR.
    OCR2B = kCountsPerTick - 2; 
    // Interrupt on A match.
    TIMSK2 = L(OCIE2B) | H(OCIE2A) | L(TOIE2);
    // Clear pending Compare A interrupts.
    TIFR2 = L(OCF2B) | H(OCF2A) | L(TOV2);
  }

  // Call once at the begining of the program.
  void init() {
    initLinPins();
    initTimer();
  }

  // Set timer value to zero.
  static inline void resetTimer() {
    // TODO: also clear timer2 prescaler.
    TCNT2 = 0;
  }
  
  // Set timer value to hals a tick.
  static inline void setTimerToHalfTick() {
    // TODO: also clear timer2 prescaler.
    TCNT2 = kCountsPerTick / 2;
  }
  
  // Return non zero if RX is passive (high), return zero if 
  // asserted (low).
  static inline uint8 isRxPassive() {
    return PIND & kRxPinMask;
  }

  // Perform a tight busy loop until RX is active or the given number
  // of clock ticks passed (timeout). Retuns true if RX is active,
  // false if timeout. Keeps timer reset during the wait.
  static inline boolean waitForRxActive(uint8 maxClockTicks) {
    const uint16 base_clock = clock::hardware_ticks_mod_16_bit();
    for(;;) {
      resetTimer();
      if (!isRxPassive()) {
        return true;
      }
      // Should work also in case of an clock overflow.
      const uint16 clock_diff = clock::hardware_ticks_mod_16_bit() - base_clock;
      if (clock_diff >= maxClockTicks) {
        return false; 
      }
    } 
  }
  
  // Same as waitForRxActive but with reversed polarity.
  // We clone to code for time optimization.
  static inline boolean waitForRxPassive(uint8 maxClockTicks) {
    const uint16 base_clock = clock::hardware_ticks_mod_16_bit();
    for(;;) {
      resetTimer();
      if (isRxPassive()) {
        return true;
      }
      // Should work also in case of an clock overflow.
      const uint16 clock_diff = clock::hardware_ticks_mod_16_bit() - base_clock;
      if (clock_diff >= maxClockTicks) {
        return false; 
      }
    } 
  }

  // Interrupt on Timer 2 A-match.
  ISR(TIMER2_COMPA_vect)
  {
    leds::on();

    // TODO: sometimes does not work withot 'volatile' ???
    static uint8 sync_counter = 0;

    if (isRxPassive()) {
      leds::off1();
      sync_counter = 0;
      //leds::off2();
    } 
    else {
      leds::on1();
      if (sync_counter >= 10) {
        leds::on2();
        waitForRxPassive(255);
        waitForRxActive(255);
        leds::off2();
        setTimerToHalfTick();
        sync_counter = 0;
      } 
      else {
        sync_counter++;
      }
    }
    
    leds::off();
  }
}

