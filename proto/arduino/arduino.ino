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

#include "serial_printer.h"
#include "avr_util.h"
#include "hardware_clock.h"
#include "system_clock.h"
#include "lin_decoder.h"
#include "io_pins.h"
#include "action_led.h"

// Auxilary LEDs.
static io_pins::OutputPin status1_led(PORTD, 6);
static io_pins::OutputPin status2_led(PORTD, 7);

// Action LEDs. Indicates activity by blinking. Require periodic calls to
// update().
static ActionLed frame_led(PORTB, 0);
static ActionLed error_led(PORTB, 1);

// Arduino standard LED.
static io_pins::OutputPin led(PORTB, 5);

void setup()
{
  // We don't want interrupts from timer 2.
  avr_util::timer0_off();

  // Uses Timer1, no interrupts.
  hardware_clock::setup();

  // Hard coded to 115.2k baud. Uses URART0, no interrupts.
  SerialPrinter.setup();

  // Uses Timer2 with interrupts, and a few i/o pins. See code for details.
  lin_decoder::setup();

  // Enable global interrupts. We expect to have only timer1 interrupts by
  // the lin decoder to reduce ISR jitter.
  sei(); 

  SerialPrinter.println(F("Setup completed"));
}

// This is a quick loop that does not use delay() or other busy loops or blocking calls.
void loop()
{
  // Periodic updates.
  system_clock::loop();
  SerialPrinter.loop();
  frame_led.loop();
  error_led.loop();

  // Generate periodic messages.
  {  
    static uint32_t last_reported_millis = 0;
    const uint32_t time_millis = system_clock::timeMillis();
    if (time_millis >= (last_reported_millis + 1000 )) {
      last_reported_millis = time_millis;
      SerialPrinter.println((uint32)time_millis);
    }
  }

  // Handle LIN errors.
  if (lin_decoder::getAndClearErrorFlag()) {
    error_led.action();
  }

  // Handle recieved LIN frames.
  lin_decoder::RxFrameBuffer buffer;
  if (readNextFrame(&buffer)) {
    frame_led.action();
    // Dump frame.
    for (int i = 0; i < buffer.num_bytes; i++) {
      if (i > 0) {
        SerialPrinter.print(' ');  
      }
      SerialPrinter.printHexByte(buffer.bytes[i]);  
    }
    SerialPrinter.println();
  }
}

