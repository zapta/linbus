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
#include "leds.h"
#include "clock.h"
#include "lin_decoder.h"

void setup()
{
   // Uses PB5, Arduino pin 13.
  leds::init();
  
  // We don't want interrupts from timer 2.
  avr_util::timer0_off();

  // Uses Timer1, no interrupt.
  clock::init();

  // Hard coded 115.2k baud. Uses URART0, no interrupts.
  SerialPrinter.init();

  // Uses Timer2 with interrupts, PB2, PD2, PD3, PD4.
  lin_decoder::init();

  // Enable interrupts.
  sei(); 

  SerialPrinter.println(F("Setup completed"));
}

void loop()
{
  SerialPrinter.update();

  static uint32_t last_reported_millis = 0;
  const uint32_t time_millis = clock::update_millis();

  if (time_millis >= (last_reported_millis + 200 )) {
    SerialPrinter.println(time_millis);
    last_reported_millis = time_millis;
  }
}

