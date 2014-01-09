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
#include "hardware_clock.h"

// ----- Baud rate related parameters. ---

// The nominal baud rate. Tested with 9600, 10000, 19200, 20000.
#define BAUD 9600

// The pre scaler of timer 2 for generating serial bit ticks.
#define PRE_SCALER 8

// Timer 2 counts for a single serial data tick. Should be <= 256.
#define COUNTS_PER_BIT ((16000000 / PRE_SCALER) / BAUD)

// ----- Debugging outputs

namespace lin_decoder {
  // 9600 baud -> 26, 20000 baud -> 12. Not bothering with rounding.
  static const uint8 kClockTicksPerBit = (hardware_clock::kTicksPerMilli * 1000) / BAUD;

  // ----- Digital I/O pins
  //
  // NOTE: we use direct register access instead the abstractions in io_pins.h. 
  // This way we shave a few cycles from the ISR.

  // RX - input, PD2. Lin bus input.
  namespace rx_pin {
    static const uint8 kPinMask  = H(PIND2);
    static inline void setup() {
      DDRD &= ~kPinMask;  // input
      PORTD |= kPinMask;  // pullup
    }
    static inline uint8 isHigh() {
      return  PIND & kPinMask;
    }
  }  // namespace rx_pin

  // BREAK - output, PC0. Indicates detection of a break. For debugging.
  namespace break_pin {
    static const uint8 kPinMask  = H(PINC0);
    static inline void setup() {
      DDRC |= kPinMask;    // output
      PORTC &= ~kPinMask;  // low
    }
    static inline void setHigh() {
      PORTC |= kPinMask;
    }
    static inline void setLow() {
      PORTC &= ~kPinMask;
    }
  } 

  // SAMPLE - output, PC1. Indicates input bit sampling. For debugging.
  namespace sample_pin {
    static const uint8 kPinMask  = H(PINC1);
    static inline void setup() {
      DDRC |= kPinMask;    // output
      PORTC &= ~kPinMask;  // low
    }
    static inline void setHigh() {
      PORTC |= kPinMask;
    }
    static inline void setLow() {
      PORTC &= ~kPinMask;
    }
  } 

  // ERROR - output, PC2. Indicates errors detection. For debugging.
  namespace error_pin {
    static const uint8 kPinMask  = H(PINC2);
    static inline void setup() {
      DDRC |= kPinMask;    // output
      PORTC &= ~kPinMask;  // low
    }
    static inline void setHigh() {
      PORTC |= kPinMask;
    }
    static inline void setLow() {
      PORTC &= ~kPinMask;
    }
  }

  // ISR - output, PC3. Indicate ISR execution period. For debugging.
  namespace isr_pin {
    static const uint8 kPinMask  = H(PINC3);
    static inline void setup() {
      DDRC |= kPinMask;    // output
      PORTC &= ~kPinMask;  // low
    }
    static inline void setHigh() {
      PORTC |= kPinMask;
    }
    static inline void setLow() {
      PORTC &= ~kPinMask;
    }
  }

  // Called one during initialization.
  static inline void setupPins() {
    rx_pin::setup();
    break_pin::setup();
    sample_pin::setup();
    error_pin::setup();
    isr_pin::setup();
  }

  // ----- ISR To Main Data Transfer -----

  // When true, request_buffer has data that should be read by main. When false, ISR
  // can fill buffer with data, if available.
  static volatile boolean request_buffer_has_data;

  // The ISR to main transfer buffer. 
  // TODO: why having volatile here breaks the compilation?
  static RxFrameBuffer request_buffer;

  // Public. Called from main. See .h for description.
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
  static inline void setupBuffers() {
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
    // Number of bits read so far in the current byte. Includes start bit, 
    // 8 data bits and one stop bits.
    static uint8 bits_read_in_byte_;
    // Buffer for the current byte we collect.
    static uint8 byte_buffer_;
    // When collecting the data bits, this goes (1 << 0) to (1 << 7). Could
    // be computed as (1 << (bits_read_in_byte_ - 1)). We use this cached value
    // recude ISR computation.
    static uint8 byte_buffer_bit_mask_;
  };

  // ----- Error Flag. -----

  // Written from ISR. Read/Write from main.
  // TODO: make this a bit or of multiple error types.
  static volatile boolean error_flag;

  // Private. Called from ISR.
  static inline void setErrorFlag() {
    error_pin::setHigh();
    error_flag = true;
    error_pin::setLow();
  }

  // Called from main. Public.
  boolean getAndClearErrorFlag() {
    // TODO: make this atomic, without increasing ISR jitter.
    const boolean result = error_flag;
    error_flag = false;
    return result;
  }

  // ----- Initialization -----

  static void setupTimer() {    
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
  void setup() {
    setupPins();
    setupBuffers();
    StateDetectBreak::enter();
    setupTimer();
    error_flag = false;
  }

  // ----- ISR Utility Functions -----

  // Set timer value to zero.
  static inline void resetTickTimer() {
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

  // Perform a tight busy loop until RX is low or the given number
  // of clock ticks passed (timeout). Retuns true if ok,
  // false if timeout. Keeps timer reset during the wait.
  static inline boolean waitForRxLow(uint16 maxClockTicks) {
    const uint16 base_clock = hardware_clock::ticks();
    for(;;) {
      // Keep the tick timer not ticking (no ISR).
      resetTickTimer();

      // If rx is low we are done.
      if (!rx_pin::isHigh()) {
        return true;
      }

      // Test for timeout.
      // Should work also in case of 16 bit clock overflow.
      const uint16 clock_diff = hardware_clock::ticks() - base_clock;
      if (clock_diff >= maxClockTicks) {
        return false; 
      }
    } 
  }

  // Same as waitForRxLow but with reversed polarity.
  // We clone to code for time optimization.
  static inline boolean waitForRxHigh(uint16 maxClockTicks) {
    const uint16 base_clock = hardware_clock::ticks();
    for(;;) {
      resetTickTimer();
      if (rx_pin::isHigh()) {
        return true;
      }
      // Should work also in case of an clock overflow.
      const uint16 clock_diff = hardware_clock::ticks() - base_clock;
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
      // This copies the request buffer struct.
      request_buffer = rx_frame_buffers[tail_frame_buffer];
      incrementTailFrameBuffer();
      request_buffer_has_data = true;
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
    if (rx_pin::isHigh()) {
      low_bits_counter_ = 0;
      return;
    } 

    // Here RX is low (active)    
    if (++low_bits_counter_ < 10) {
      return;
    }

    // Detected a break. Wait for rx high and enter data reading.
    break_pin::setHigh();
    // TODO: set actual max count
    waitForRxHigh(255);
    break_pin::setLow();
    StateReadData::enter();
  }

  // ----- Read-Data State Implementation -----

  uint8 StateReadData::bytes_read_;
  uint8 StateReadData::bits_read_in_byte_;
  uint8 StateReadData::byte_buffer_;
  uint8 StateReadData::byte_buffer_bit_mask_;

  // Called after a long break changed to high.
  inline void StateReadData::enter() {
    state = READ_DATA;
    bytes_read_ = 0;
    bits_read_in_byte_ = 0;
    rx_frame_buffers[head_frame_buffer].num_bytes = 0;

    // TODO: handle post break timeout errors.
    // TODO: set a reasonable time limit.
    waitForRxLow(255);
    setTimerToHalfTick();   
  }

  inline void StateReadData::handle_isr() {
    // Sample data bit ASAP to avoid jitter.
    sample_pin::setHigh();
    const uint8 is_rx_high = rx_pin::isHigh();
    sample_pin::setLow();

    // Handle start bit.
    if (bits_read_in_byte_ == 0) {
      // Start bit error.
      if (is_rx_high) {
        setErrorFlag();
        StateDetectBreak::enter();
        return;
      }  
      // Start bit ok.
      bits_read_in_byte_++;
      // Prepare buffer and mask for data bit collection.
      byte_buffer_ = 0;
      byte_buffer_bit_mask_ = (1 << 0);
      return;
    }

    // Handle next, out of 8, data bits. Collect the current bit into byte_buffer_, lsb first.
    if (bits_read_in_byte_ <= 8) {
      if (is_rx_high) {
        byte_buffer_ |= byte_buffer_bit_mask_;
      }
      byte_buffer_bit_mask_ = byte_buffer_bit_mask_ << 1;
      bits_read_in_byte_++;
      return;
    }

    // Here when stop bit. Error if not high.
    if (!is_rx_high) {
      setErrorFlag();
      StateDetectBreak::enter();
      return;
    }  

    // Handle sync byte.
    if (bytes_read_ == 0) {
      // Should be exactly 0x55. We don't append this byte to the buffer.
      if (byte_buffer_ != 0x55) {
        setErrorFlag();
        StateDetectBreak::enter();
        return;
      }
    } 
    // Handle non sync byte. We append it to the buffer.
    else {
      // NOTE: the byte limit count is enforeced somewhere else so we can assume safely here that this 
      // will not cause a buffer overlow.
      RxFrameBuffer* const frame_buffer = &rx_frame_buffers[head_frame_buffer];
      frame_buffer->bytes[frame_buffer->num_bytes++] = byte_buffer_;
    }

    // Preper for next byte. We will reset the byte_buffer in the next start
    // bit, not here.
    bytes_read_++;
    bits_read_in_byte_ = 0;

    // Wait for the high to low transition of start bit of next byte.
    const boolean has_more_bytes =  waitForRxLow(kClockTicksPerBit * 4);

    // Handle the case of no transition. We just read the last byte in this frame.
    if (!has_more_bytes) {
      // Verify min byte count.
      if (bytes_read_ < RxFrameBuffer::kMinBytes) {
        setErrorFlag();
        StateDetectBreak::enter();
        return;
      }

      // Frame looks ok so far. Move to next frame in the ring buffer.
      // NOTE: we will reset the byte_count of the new frame buffer next time we will enter data detect state.
      // NOTE: verification of sync byte, id, checksum, etc is done latter by the main code, not the ISR.
      incrementHeadFrameBuffer();
      if (tail_frame_buffer == head_frame_buffer) {
        // Frame buffer overrun.        
        setErrorFlag();
        incrementTailFrameBuffer();
      }

      StateDetectBreak::enter();
      return;
    }

    // Here when there is at least one more byte in this frame. Error if we already had
    // the max number of bytes.
    if (rx_frame_buffers[head_frame_buffer].num_bytes >= RxFrameBuffer::kMaxBytes) {
      setErrorFlag();
      StateDetectBreak::enter();
      return;  
    }

    // Everything is ready for the next byte. Have a tick in the middle of its
    // start bit.
    //
    // TODO: move this to above the num_bytes check above for more accurate 
    // timing of mid bit tick?    
    setTimerToHalfTick();
  }

  // ----- ISR Handler -----

  // Interrupt on Timer 2 A-match.
  ISR(TIMER2_COMPA_vect)
  {
    isr_pin::setHigh();

    // TODO: make this state a boolean instead of enum? (efficency).
    switch (state) {
    case DETECT_BREAK:
      StateDetectBreak::handle_isr();
      break;
    case READ_DATA:
      StateReadData::handle_isr();
      break;
    default:
      setErrorFlag();
      StateDetectBreak::enter();
    }

    // This should be fast enough even when setting in start bit half bit
    // interupt period. Otherwise, we can avoid calling this in an ISR
    // cycle that called setTimerToHalfTick(). 
    maybeServiceRxRequest();
    isr_pin::setLow();
  }
}  // namespace lin_decoder


