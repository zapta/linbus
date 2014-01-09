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
static ActionLed status1_led(PORTD, 6);
static io_pins::OutputPin status2_led(PORTD, 7);

// Action LEDs. Indicates activity by blinking. Require periodic calls to
// update().
static ActionLed frame_led(PORTB, 0);
static ActionLed error_led(PORTB, 1);

// Arduino standard LED.
static io_pins::OutputPin led(PORTB, 5);

// Compute frame checksum. 
static uint8 frameChecksum(const lin_decoder::RxFrameBuffer& buffer) {
  // LIN V1 and V2 have slightly different checksum formulas.
  static const boolean kV2Checksum = false;

  // LIN V2 includes ID byte in checksum, V1 does not.
  const uint8 startByte = kV2Checksum ? 0 : 1;
  const uint8* p = &buffer.bytes[startByte];
  // Exclude also the checksum at the end.
  uint8 nBytes = buffer.num_bytes - (startByte + 1);

  // Sum bytes. We should not have 16 bit overflow here since the frame has a limited size.
  uint16 sum = 0;
  while (nBytes-- > 0) {
    sum += *(p++);
  }

  // Keep adding the high and low bytes until no carry.
  for (;;) {
    const uint8 highByte = (uint8)(sum >> 8);
    if (!highByte) {
      break;  
    }
    // NOTE: this can add additional carry.  
    sum = (sum & 0xff) + highByte; 
  }

  return (uint8)(~sum);
}

static boolean isFrameValid(const lin_decoder::RxFrameBuffer& buffer) {
  const uint8 n = buffer.num_bytes;
  
  // NOTE: min and max frame sizes are already validated by the lin decoder ISR.
  
  // Check frame checksum byte.
  if (buffer.bytes[n - 1] != frameChecksum(buffer)) {
    return false;
  }  
  // TODO: check protected id.
  return true;
}

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

  SerialPrinter.println(F("\nSetup completed"));
}

// This is a quick loop that does not use delay() or other busy loops or blocking calls.
// The iterations are are at the order of 60usec.
void loop()
{
  // Having our own loop shaves about 4 usec per iteration. It also eliminate
  // any underlying functionality that we may not want.
  for(;;) {
    led.high();

    // Periodic updates.
    system_clock::loop();
    SerialPrinter.loop();
    status1_led.loop();
    frame_led.loop();
    error_led.loop();

    // Heart beat led.
    {
      static PassiveTimer heart_beat_timer;
      if (heart_beat_timer.timeMillis() >= 3000) {
        status1_led.action(); 
        heart_beat_timer.restart(); 
      }
    }

    // Generate periodic messages.
    static PassiveTimer periodic_watchdog;
    static byte pending_chars = 0;
    if (periodic_watchdog.timeMillis() >= 1000) {
      if (pending_chars >= 10) {
        SerialPrinter.println();
        pending_chars = 0;
      }
      periodic_watchdog.restart();
      SerialPrinter.print(F("."));
      pending_chars++;
    }

    // Handle LIN errors.
    if (lin_decoder::getAndClearErrorFlag()) {
      error_led.action();
    }

    // Handle recieved LIN frames.
    lin_decoder::RxFrameBuffer buffer;
    if (readNextFrame(&buffer)) {
      if (pending_chars) {
        SerialPrinter.println();
        pending_chars = 0;
      }
      const boolean frameOk = isFrameValid(buffer);
      if (frameOk) {
        frame_led.action();
      } 
      else {
        error_led.action();
      }
      // Dump frame.
      for (int i = 0; i < buffer.num_bytes; i++) {
        if (i > 0) {
          SerialPrinter.print(' ');  
        }
        SerialPrinter.printHexByte(buffer.bytes[i]);  
      }
      SerialPrinter.println(frameOk ? F(" OK") : F(" ER"));  
      periodic_watchdog.restart(); 
    }
    led.low();
  }
}



