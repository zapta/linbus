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

#include "avr_util.h"
#include "custom_defs.h"
#include "custom_module.h"
#include "hardware_clock.h"
#include "io_pins.h"
#include "leds.h"
#include "lin_processor.h"
#include "sio.h"
#include "system_clock.h"

// Arduino setup function. Called once during initialization.
void setup()
{
  // Hard coded to 115.2k baud. Uses URART0, no interrupts.
  // Initialize this first since some setup methods uses it.
  sio::setup();

  // Uses Timer1, no interrupts.
  hardware_clock::setup();

  // Uses Timer2 with interrupts, and a few i/o pins. See source code for details.
  lin_processor::setup();
  
  custom_module::setup();

  // Enable global interrupts. We expect to have only timer1 interrupts by
  // the lin processor to reduce ISR jitter.
  sei(); 
  
  // Have an early 'waiting' led bling to indicate normal operation.
  leds::frames.action(); 
}

// Arduino loop() method. Called after setup(). Never returns.
// This is a quick loop that does not use delay() or other busy loops or 
// blocking calls.
void loop()
{
  // Having our own loop shaves about 4 usec per iteration. It also eliminate
  // any underlying functionality that we may not want.
  for(;;) {    
    // Periodic updates.
    system_clock::loop();    
    sio::loop();
    leds::loop(); 
    custom_module::loop();

    // Print a periodic text messages if no activiy.
    static PassiveTimer idle_timer;
    if (idle_timer.timeMillis() >= 3000) {
      // Slow blinking indicates waiting.
      leds::frames.action(); 
      sio::println(F("waiting..."));
      idle_timer.restart();
    }

    // Handle LIN processor error flags.
    {
      // Used to trigger periodic error printing.
      static PassiveTimer lin_errors_timeout;
      // Accomulates error flags until next printing.
      static uint8 pending_lin_errors = 0;
      
      const uint8 new_lin_errors = lin_processor::getAndClearErrorFlags();
      if (new_lin_errors) {
        // Make the ERRORS led blinking.
        leds::errors.action();
        idle_timer.restart();
      }

      // If pending errors and time to print errors then print and clear.
      pending_lin_errors |= new_lin_errors;
      if (pending_lin_errors && lin_errors_timeout.timeMillis() > 1000) {
        sio::print(F("LIN errors: "));
        lin_processor::printErrorFlags(pending_lin_errors);
        sio::println();
        lin_errors_timeout.restart();
        pending_lin_errors = 0;
      }
    }
    
    // Handle recieved LIN frames.
    LinFrame frame;
    if (lin_processor::readNextFrame(&frame)) {
      const boolean frameOk = frame.isValid();
      if (frameOk) {
        // Make the FRAMES led blinking.
        leds::frames.action();
      } 
      else {
        // Make the ERRORS frame blinking.
        leds::errors.action();
      }
      
      // Print frame to serial port.
      for (int i = 0; i < frame.num_bytes(); i++) {
        if (i > 0) {
          sio::printchar(' ');  
        }
        sio::printhex2(frame.get_byte(i));  
      }
      if (frame.hasInjectedBits()) {
        sio::print(F(" *"));
      }
      if (!frameOk) {
        sio::print(F(" ERR"));
      }
      sio::println();  
      // Supress the 'waiting' messages.
      idle_timer.restart(); 

      // Inform the custom module about the incoming frame in case it
      // needs to intercept signals. This call by itself does not do signal
      // injection since the frame was already transfered. However, the custom
      // module can use it to influence injection of future frames.
      if (frameOk) {
        custom_module::frameArrived(frame);
      }
    }
  }
}

