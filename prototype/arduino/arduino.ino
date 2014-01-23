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

#include "action_led.h"
#include "avr_util.h"
#include "action_buzzer.h"
#include "hardware_clock.h"
#include "io_pins.h"
#include "lin_decoder.h"
#include "sio.h"
#include "system_clock.h"

// Config for P981/Cayman.
static const uint16 kLinSpeed = 19200;
static const boolean kUseLinV2Checksum = true;

// Config pin. Sampled once during initialization and does not change value
// after that. Slects one of two configurations. Default configuration is when
// high (pullup input left unconnected).
//static io_pins::ConfigInputPin alt_config_pin(PORTB, 2);

// Status LEDs.
static io_pins::OutputPin status_led(PORTD, 6);

// Action LEDs. Indicates activity by blinking. Require periodic calls to
// update().
static ActionLed frames_activity_led(PORTB, 0);
static ActionLed errors_activity_led(PORTB, 1);
static ActionLed waiting_activity_led(PORTD, 7);

void setup()
{
  // Hard coded to 115.2k baud. Uses URART0, no interrupts.
  // Initialize this first since some setup methods uses it.
  sio::setup();

  // Init buzzer. Leaves in off state.
  action_buzzer::setup();

  // Uses Timer1, no interrupts.
  hardware_clock::setup();

  // Uses Timer2 with interrupts, and a few i/o pins. See code for details.
  lin_decoder::setup(kLinSpeed);

  // Enable global interrupts. We expect to have only timer1 interrupts by
  // the lin decoder to reduce ISR jitter.
  sei(); 
}

// This is a quick loop that does not use delay() or other busy loops or blocking calls.
// The iterations are are at the or  sio::waitUntilFlushed();
void loop()
{
  // Having our own loop shaves about 4 usec per iteration. It also eliminate
  // any underlying functionality that we may not want.
  for(;;) {    
    // Periodic updates.
    system_clock::loop();    
    sio::loop();
    action_buzzer::loop();
    waiting_activity_led.loop();
    frames_activity_led.loop();
    errors_activity_led.loop();  

    // Generate periodic messages if no activiy.
    static PassiveTimer idle_timer;
    if (idle_timer.timeMillis() >= 3000) {
      waiting_activity_led.action(); 
      sio::println(F("waiting..."));
      idle_timer.restart();
    }

    // Handle LIN decoder error flags.
    {
      static PassiveTimer lin_errors_timeout;
      static uint8 pending_lin_errors = 0;
      const uint8 new_lin_errors = lin_decoder::getAndClearErrorFlags();
      if (new_lin_errors) {
        errors_activity_led.action();
        idle_timer.restart();
      }

      pending_lin_errors |= new_lin_errors;
      if (pending_lin_errors && lin_errors_timeout.timeMillis() > 1000) {
        sio::print(F("LIN errors: "));
        lin_decoder::printErrorFlags(pending_lin_errors);
        sio::println();
        lin_errors_timeout.restart();
        pending_lin_errors = 0;
      }
    }

    // Handle recieved LIN frames.
    LinFrame frame;
    if (lin_decoder::readNextFrame(&frame)) {
      const boolean frameOk = frame.isValid(kUseLinV2Checksum);
      if (frameOk) {
        frames_activity_led.action();
      } 
      else {
        errors_activity_led.action();
      }
      // Dump frame.
      for (int i = 0; i < frame.num_bytes(); i++) {
        if (i > 0) {
          sio::printchar(' ');  
        }
        sio::printhex2(frame.get_byte(i));  
      }
      if (!frameOk) {
        sio::print(F(" ERR"));
      }
      sio::println();  
      idle_timer.restart(); 

      // Sample frame action.    
      // Handle reverse gear alarm. Tested with P981/CS when connecting to the
      // linbus near the windshiled mirror (homelink unit).
      // Test frame: 39 04 00 00 00 00 00.
      if (frameOk) {
        // Data bytes are between id and checksum bytes.
        const uint8 data_size = frame.num_bytes() - 2;
        const uint8 id = frame.get_byte(0);
        if (data_size == 6 && id == 0x39) {
          const boolean reverse_gear = frame.get_byte(1) & H(2);
          status_led.set(reverse_gear);
          action_buzzer::action(reverse_gear);  
        }
      }
    }
  }
}

