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



// Config pin. Sampled once during initialization does not change value
// after that. Using alternative configuration when pin is low.
static io_pins::ConfigInputPin alt_config_pin(PORTB, 2);

// Auxilary LEDs.
static ActionLed status1_led(PORTD, 6);
static io_pins::OutputPin status2_led(PORTD, 7);

// Action LEDs. Indicates activity by blinking. Require periodic calls to
// update().
static ActionLed frame_led(PORTB, 0);
static ActionLed error_led(PORTB, 1);

// Arduino standard LED.
static io_pins::OutputPin led(PORTB, 5);

// Compute frame checksum. Assuming buffer is not empty.
static uint8 frameChecksum(const lin_decoder::RxFrameBuffer& buffer, boolean use_lin_v2_checksum) {
  // LIN V2 includes ID byte in checksum, V1 does not.
  // Per the assumption above, we have at least one byte.
  const uint8 startByte = use_lin_v2_checksum ? 0 : 1;
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

// Replace the 2 msb bits with checksum of the node ID in the 6 lsb bits per the
// LINBUS spec.
static uint8 setIdChecksumBits(uint8 id) {
  // Algorithm is optimized for CPU time (avoiding individual shifts per id bit).
  // Using registers for the two checksum bits. P1 is computed in bit 7 of p1_at_b7 
  // and p0 is comptuted in bit 6 of p0_at_b6.
  uint8 p1_at_b7 = ~0;
  uint8 p0_at_b6 = 0;

  // P1: id5, P0: id4
  uint8 shifter = id << 2;
  p1_at_b7 ^= shifter;
  p0_at_b6 ^= shifter;

  // P1: id4, P0: id3
  shifter += shifter;
  p1_at_b7 ^= shifter;

  // P1: id3, P0: id2
  shifter += shifter;
  p1_at_b7 ^= shifter;
  p0_at_b6 ^= shifter;

  // P1: id2, P0: id1
  shifter += shifter;
  p0_at_b6 ^= shifter;

  // P1: id1, P0: id0
  shifter += shifter;
  p1_at_b7 ^= shifter;
  p0_at_b6 ^= shifter;

  return (p1_at_b7 & 0b10000000) | (p0_at_b6 & 0b01000000) | (id & 0b00111111);
}

// We consider a single byte frame with ID only and no slave response as valid.
static boolean isFrameValid(const lin_decoder::RxFrameBuffer& buffer, boolean use_lin_v2_checksum) {
  const uint8 n = buffer.num_bytes;

  // Check frame size.
  // One ID byte with optional 1-8 data bytes and 1 checksum byte.
  // TODO: should we enforce only 1, 2, 4, or 8 data bytes?  (total size 
  // 1, 3, 4, 6, or 10)
  if (n != 1 && (n < 3 || n > 10)) {
    return false;
  }

  // Check ID byte checksum bits.
  const uint8 id_byte = buffer.bytes[0];
  if (id_byte != setIdChecksumBits(id_byte)) {
    // TODO: remove after stabilization.
    // SerialPrinter.printHexByte(id_byte);
    // SerialPrinter.print(" vs. ");
    // SerialPrinter.printHexByte(setIdChecksumBits(id_byte));
    // SerialPrinter.println();
    return false;
  }

  // If not an ID only frame, check also the overall checksum.
  if (n > 1) {
    if (buffer.bytes[n - 1] != frameChecksum(buffer, use_lin_v2_checksum)) {
      return false;
    }  
  }
  // TODO: check protected id.
  return true;
}

// TODO: move to lin_decoder.
struct BitName {
  uint8 mask;
  char *name;  
};

// TODO: move to lin_decoder.
static const  BitName kErrorBitNames[] PROGMEM = {
  { lin_decoder::ERROR_FRAME_TOO_SHORT, "SHRT" },
  { lin_decoder::ERROR_FRAME_TOO_LONG, "LONG" },
  { lin_decoder::ERROR_START_BIT, "STRT" },
  { lin_decoder::ERROR_STOP_BIT, "STOP" },
  { lin_decoder::ERROR_SYNC_BYTE, "SYNC" },
  { lin_decoder::ERROR_BUFFER_OVERRUN, "OVRN" },
  { lin_decoder::ERROR_OTHER, "OTHR" },
};

// Given a byte with lin decoder error bitset, print the list
// of set errors.
// TODO: move to lin_decoder.
static void printLinErrors(uint8 lin_errors) {
  const uint8 n = ARRAY_SIZE(kErrorBitNames); 
  boolean any_printed = false;
  for (uint8 i = 0; i < n; i++) {
    const uint8 mask = pgm_read_byte(&kErrorBitNames[i].mask);
    if (lin_errors & mask) {
      if (any_printed) {
        sio::printchar(' ');
      }
      const char* const name = (const char*)pgm_read_word(&kErrorBitNames[i].name);
      sio::print(name);
      any_printed = true;
    }
  }
}

void setup()
{
  // Hard coded to 115.2k baud. Uses URART0, no interrupts.
  // Initialize this first since some setup methods uses it.
  sio::setup();

  // If alt config pin is pulled low, using alternative config..
  const boolean alt_config = alt_config_pin.isLow();
  sio::waitUntilFlushed();
  sio::print(F("Config: "));
  sio::print(alt_config ? F("ALT") : F("STD"));
  sio::println();

  // Init buzzer. Leaves in off state.
  action_buzzer::setup();

  // Uses Timer1, no interrupts.
  hardware_clock::setup();

  // Uses Timer2 with interrupts, and a few i/o pins. See code for details.
  lin_decoder::setup(alt_config ? 9600 : 19200);

  // Enable global interrupts. We expect to have only timer1 interrupts by
  // the lin decoder to reduce ISR jitter.
  sei(); 
}

// This is a quick loop that does not use delay() or other busy loops or blocking calls.
// The iterations are are at the or  sio::waitUntilFlushed();
void loop()
{
  const boolean use_lin_v2_checksum = alt_config_pin.isHigh();
  sio::print(F("Checksum: "));
  sio::println(use_lin_v2_checksum ? F("V2") : F("V1"));

  // Having our own loop shaves about 4 usec per iteration. It also eliminate
  // any underlying functionality that we may not want.
  for(;;) {
    // Periodic updates.
    system_clock::loop();
    sio::loop();
    action_buzzer::loop();
    status1_led.loop();
    frame_led.loop();
    error_led.loop();  

    // Generate periodic messages if no activiy.
    static PassiveTimer idle_timer;
    if (idle_timer.timeMillis() >= 3000) {
      status1_led.action(); 
      sio::println(F("waiting..."));
      idle_timer.restart();
    }

    // Handle LIN errors.
    {
      static PassiveTimer lin_errors_timeout;
      static uint8 pending_lin_errors = 0;
      const uint8 new_lin_errors = lin_decoder::getAndClearErrorFlag();
      if (new_lin_errors) {
        error_led.action();
        idle_timer.restart();
      }

      pending_lin_errors |= new_lin_errors;
      if (pending_lin_errors && lin_errors_timeout.timeMillis() > 1000) {
        sio::print(F("LIN errors: "));
        printLinErrors(pending_lin_errors);
        sio::println();
        lin_errors_timeout.restart();
        pending_lin_errors = 0;
      }
    }

    // Handle recieved LIN frames.
    lin_decoder::RxFrameBuffer buffer;
    if (readNextFrame(&buffer)) {
      const boolean frameOk = isFrameValid(buffer, use_lin_v2_checksum);
      if (frameOk) {
        frame_led.action();
      } 
      else {
        error_led.action();
      }
      // Dump frame.
      for (int i = 0; i < buffer.num_bytes; i++) {
        if (i > 0) {
          sio::printchar(' ');  
        }
        sio::printhex2(buffer.bytes[i]);  
      }
      if (!frameOk) {
        sio::print(F(" ERR"));
      }
      sio::println();  
      idle_timer.restart(); 

      // Sample frame action.    
      // Handle reverse gear alarm. Tested with P981/CS when connecting to the
      // linbus near the windshiled mirror (homelink unit).
      // Test frame: 39 04 00 00 00.
      if (frameOk) {
        // Data bytes are between id and checksum bytes.
        const uint8 data_size = buffer.num_bytes - 2;
        const uint8 id = buffer.bytes[0];
        if (data_size == 4 && id == 0x39 && (buffer.bytes[1] & H(2))) {
          action_buzzer::action();  
        }
      }
    }
  }
}


