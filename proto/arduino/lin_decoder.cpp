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

// TODO: file is too long. Refactor.

#include "lin_decoder.h"
#include "avr_util.h"
#include "leds.h"
#include "clock.h"

// ----- Baud rate related parameters. ---

// The nominal baud rate. Tested with 9600, 10000, 19200, 20000.
#define BAUD 9600

// The pre scaler of timer 2 for generating serial bit ticks.
#define PRE_SCALER 8

// Timer 2 counts for a single serial data tick. Should be <= 256.
#define COUNTS_PER_BIT (16000000/PRE_SCALER/BAUD)

// ----- Debugging outputs

// High while servicing the ISR.
#define DEBUG_LED_IN_ISR_ON leds::on1()
#define DEBUG_LED_IN_ISR_OFF leds::off1()

// High when a break was detected.
#define DEBUG_LED_BREAK_ON leds::on2()
#define DEBUG_LED_BREAK_OFF leds::off2()

// A short pulse when sampling a data bit.
#define DEBUG_LED_DATA_BIT_ON leds::on3()
#define DEBUG_LED_DATA_BIT_OFF leds::off3()

// A short pulse when incrementing the error counter.
#define DEBUG_LED_ERROR_ON leds::on4()
#define DEBUG_LED_ERROR_OFF leds::off4()
  
namespace lin_decoder {
  // 10k -> 25, 20000 baud -> 12.
  static const uint8 kClockTicksPerBit = clock::kHardwareTicksPerSecond / BAUD;

  // Pins for communicating with the LIN transceiver.
  static const uint8 kRxPinMask  = H(PIND2);
  static const uint8 kTxPinMask = H(PINB2);
  static const uint8 kEnPinMask = H(PIND4);
  
  // ----- ISR To Main Data Transfer -----

  // When true, request_buffer has data that should be read by main.
  static volatile boolean request_buffer_has_data;
  
  // TODO: why making this volatile breaks the compilation?
  static RxFrameBuffer request_buffer;
  
    // Public. Called from main.
  boolean readNextFrame(RxFrameBuffer* buffer) {
    if (request_buffer_has_data) {
      *buffer = request_buffer;
      request_buffer_has_data = false;
      return true;  
    }  
    return false; 
  }
  
  // ----- ISR RX Ring Buffers -----

  // Frame buffer queue size.
  static const uint8 kMaxFrameBuffers = 8;
  
  // RX Frame buffers queue. Read/Writen by ISR only. 
  static RxFrameBuffer rx_frame_buffers[kMaxFrameBuffers];
  
  // Index [0, kMaxFrameBuffers) of the current frame buffer being
  // written (newest). Read/Written by ISR only.
  static uint8  head_frame_buffer;
  
  // Index [0, kMaxFrameBuffers) of the next frame to be read (oldest).
  // If equals head_frame_buffer then there is no available frame.
  // Read/Written by ISR only.
  static uint8 tail_frame_buffer;
  
  // Called once from main.
  static inline void initBuffers() {
    head_frame_buffer = 0;
    tail_frame_buffer = 0;
    request_buffer_has_data = false;
    request_buffer.num_bytes = 0;
  }
  
  // Called from main after consuming a tail buffer.
  static inline void incrementTailFrameBuffer() {
    if (++tail_frame_buffer >= kMaxFrameBuffers) {
      tail_frame_buffer = 0;
    }
  }
  
  // Called from ISR. If stepping on tail buffer, caller needs to 
  // increment raise frame overrun error.
  static inline void incrementHeadFrameBuffer() {
    if (++head_frame_buffer >= kMaxFrameBuffers) {
      head_frame_buffer = 0;
    }
  }
    
  // ----- State Machine Declaration -----

  static enum {
    DETECT_BREAK,
    READ_DATA    
  } 
  state;

  class StateDetectBreak {
   public:
    static inline void enter() ;
    static inline void handle_isr();
   private:
    static uint8 low_bits_counter_;
  };

  class StateReadData {
   public:
    // Should be called after the break stop bit was detected.
    static inline void enter();
    static inline void handle_isr();
   private:
    // Number of complete bytes read so far. Includes all bytes, even
    // sync, id and checksum.
    static uint8 bytes_read_;
    // Number of bits read so far in the current byte. Includes all bits,
    // even start and stop bits.
    static uint8 bits_read_in_byte_;
  };
  
  // ----- Error Flag. -----
  
  // Written from ISR. Read/Write from main.
  static volatile boolean errors_flag;
  
  // Private. Called from ISR.
  static inline void set_errors_flag() {
    DEBUG_LED_ERROR_ON;
    errors_flag = true;
    DEBUG_LED_ERROR_OFF;
  }
  
  // Called from main. Public.
  boolean hasErrors() {
    return errors_flag;
  }
  
  // Called from main. Public.
  void clearErrors() {
    errors_flag = false;
  }

  // ----- Initialization -----

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
    #if (PRE_SCALER != 8)
    #error "Prescaler mismatch"
    #endif
    TCCR2B = L(FOC2A) | L(FOC2B) | H(WGM22) | L(CS22) | H(CS21) | L(CS20);
    // Clear counter.
    TCNT2 = 0;
    // Determines baud rate.
    #if (COUNTS_PER_BIT > 256) 
    #error "Baud too low, counts does not fit in a byte, needs a larger prescaler."
    #endif
    OCR2A = COUNTS_PER_BIT - 1;
    // A short 8 clocks pulse on OC2B at the end of each cycle,
    // just before triggering the ISR.
    OCR2B = COUNTS_PER_BIT - 2; 
    // Interrupt on A match.
    TIMSK2 = L(OCIE2B) | H(OCIE2A) | L(TOIE2);
    // Clear pending Compare A interrupts.
    TIFR2 = L(OCF2B) | H(OCF2A) | L(TOV2);
  }

  // Call once from main at the begining of the program.
  void init() {
    initBuffers();
    StateDetectBreak::enter();
    initLinPins();
    initTimer();
    clearErrors();
  }

  // ----- ISR Utility Functions -----

  // Set timer value to zero.
  static inline void resetTimer() {
    // TODO: also clear timer2 prescaler.
    TCNT2 = 0;
  }

  // Set timer value to half a tick. Called at the begining of the
  // start bit to generate sampling ticks at the middle of the next
  // 10 bits (start, 8 * data, stop).
  static inline void setTimerToHalfTick() {
    // Adding 2 to compensate for pre calling delay. The goal is
    // to have the next ISR data sampling at the middle of the start
    // bit.
    TCNT2 = (COUNTS_PER_BIT / 2) + 2;
  }

  // Return non zero if RX is high (passive), return zero if 
  // asserted (low).
  static inline uint8 isRxHigh() {
    return PIND & kRxPinMask;
  }

  // Perform a tight busy loop until RX is low or the given number
  // of clock ticks passed (timeout). Retuns true if ok,
  // false if timeout. Keeps timer reset during the wait.
  static inline boolean waitForRxLow(uint16 maxClockTicks) {
    const uint16 base_clock = clock::hardware_ticks_mod_16_bit();
    for(;;) {
      resetTimer();
      if (!isRxHigh()) {
        return true;
      }
      // Should work also in case of an clock overflow.
      const uint16 clock_diff = clock::hardware_ticks_mod_16_bit() - base_clock;
      if (clock_diff >= maxClockTicks) {
        return false; 
      }
    } 
  }

  // Same as waitForRxLow but with reversed polarity.
  // We clone to code for time optimization.
  static inline boolean waitForRxHigh(uint16 maxClockTicks) {
    const uint16 base_clock = clock::hardware_ticks_mod_16_bit();
    for(;;) {
      resetTimer();
      if (isRxHigh()) {
        return true;
      }
      // Should work also in case of an clock overflow.
      const uint16 clock_diff = clock::hardware_ticks_mod_16_bit() - base_clock;
      if (clock_diff >= maxClockTicks) {
        return false; 
      }
    } 
  }
  
  // Called from ISR.
  static inline void maybeServiceRxRequest() {
    //  If request buffer is empty and queue has an RX frame then move it to
    // the request buffer. 
    if (!request_buffer_has_data && tail_frame_buffer != head_frame_buffer) {
      leds::on();  // @@@ remove
      // This copies the request buffer struct.
      request_buffer = rx_frame_buffers[tail_frame_buffer];
      incrementTailFrameBuffer();
      request_buffer_has_data = true;
      leds::off();   // @@@ remove
    }
  }
  
  // ----- Detect-Break State Implementation -----
  
  uint8 StateDetectBreak::low_bits_counter_;
  
  inline void StateDetectBreak::enter() {
    state = DETECT_BREAK;
    low_bits_counter_ = 0;
  }
  
  // Return true if enough time to service rx request.
  inline void StateDetectBreak::handle_isr() {
    if (isRxHigh()) {
      low_bits_counter_ = 0;
      return;
    } 
    
    // Here RX is low (active)    
    if (++low_bits_counter_ < 10) {
      return;
    }
    
    // Detected a break. Wait for rx high and enter data reading.
    DEBUG_LED_BREAK_ON;
    // TODO: set actual max count
    waitForRxHigh(255);
    DEBUG_LED_BREAK_OFF;
    StateReadData::enter();
  }

  // ----- Read-Data State Implementation -----
  
  uint8 StateReadData::bytes_read_;
  uint8 StateReadData::bits_read_in_byte_;
  
  inline void StateReadData::enter() {
    state = READ_DATA;
    bytes_read_ = 0;
    bits_read_in_byte_ = 0;
    
    // TODO: handle timeout errors.
    waitForRxLow(255);
    setTimerToHalfTick();
  }
  
  inline void StateReadData::handle_isr() {
    // Sample data bit
    DEBUG_LED_DATA_BIT_ON;
    const uint8 is_rx_high = isRxHigh();
    DEBUG_LED_DATA_BIT_OFF;
    
    // Start bit. Rx should be low.
    if (bits_read_in_byte_ == 0) {
      if (is_rx_high) {
        set_errors_flag();
        StateDetectBreak::enter();
        return;
      }  
      bits_read_in_byte_++;
      return;
    }

    // Data bit. 
    if (bits_read_in_byte_ <= 8) {
      bits_read_in_byte_++;
      return;
    }
    
    // Stop bit
    if (!is_rx_high) {
      set_errors_flag();
      StateDetectBreak::enter();
      return;
    }  
    
    // TODO: if byte 0, verify sync 0x55.
    
    // Preper for next byte.
    bytes_read_++;
    bits_read_in_byte_ = 0;
    
    // TODO: detect byte overflow (more than 8)
    
    if (!waitForRxLow(kClockTicksPerBit * 4)) {
      // No more data bytes. 
      // TODO: verify checksum
      incrementHeadFrameBuffer();
      if (tail_frame_buffer == head_frame_buffer) {
        incrementTailFrameBuffer();
      }
      
      // TODO: fill actual data.
      RxFrameBuffer* tail = &rx_frame_buffers[tail_frame_buffer];
      tail->num_bytes = 1;
      tail->bytes[0] = tail_frame_buffer;
    
      StateDetectBreak::enter();
      return;
    }
    
    // Avoid more than 11 bytes (sync, id, 8*data, checksum)
    if (bits_read_in_byte_ >= 11) {
      set_errors_flag();
      StateDetectBreak::enter();
      return;  
    }
        
    setTimerToHalfTick();
  }

  // ----- ISR Handler -----

  // Interrupt on Timer 2 A-match.
  ISR(TIMER2_COMPA_vect)
  {
    DEBUG_LED_IN_ISR_ON;

    // TODO: make this state a boolean instead of enum? (efficency).
    switch (state) {
      case DETECT_BREAK:
        StateDetectBreak::handle_isr();
        break;
      case READ_DATA:
        StateReadData::handle_isr();
        break;
      default:
        set_errors_flag();
        StateDetectBreak::enter();
    }
   
    // This should be fast enough even when setting in start bit half bit
    // interupt period. Otherwise, we can avoid calling this in an ISR
    // cycle that called setTimerToHalfTick(). 
    maybeServiceRxRequest();
    DEBUG_LED_IN_ISR_OFF;
  }
}

