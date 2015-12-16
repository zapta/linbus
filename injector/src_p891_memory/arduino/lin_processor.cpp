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

#include "lin_processor.h"

#include "avr_util.h"
#include "hardware_clock.h"
#include "custom_injector.h"

// TODO: for debugging. Remove.
#include "sio.h"

// ----- Baud rate related parameters. ---

// If an out of range speed is specified, using this one.
static const uint16 kDefaultBaud = 9600;

// Wait at most N bits from the end of the stop bit of previous byte
// to the start bit of next byte.
//
// TODO: seperate values for pre response space (longer, e.g. 8) and pre
// regular byte space (shorter, e.g. 4).
//
static const uint8 kMaxSpaceBits = 6;

// Define an input pin with fast access. Using the macro does
// not increase the pin access time compared to direct bit manipulation.
// Pin is setup with active pullup.
#define DEFINE_INPUT_PIN(name, port_letter, bit_index) \
  namespace name { \
    static const uint8 kPinMask  = H(bit_index); \
    static inline void setup() { \
      DDR ## port_letter &= ~kPinMask;  \
      PORT ## port_letter |= kPinMask;  \
    } \
    static inline uint8 isHigh() { \
      return  PIN##port_letter & kPinMask; \
    } \
  } 
 
// Define an output pin with fast access. Using the macro does
// not increase the pin access time compared to direct bit manipulation.
#define DEFINE_OUTPUT_PIN(name, port_letter, bit_index, initial_value) \
  namespace name { \
    static const uint8 kPinMask  = H(bit_index); \
    static inline void setHigh() { \
      PORT ## port_letter |= kPinMask; \
    } \
    static inline void setLow() { \
      PORT ## port_letter &= ~kPinMask; \
    } \
    static inline void setup() { \
      DDR ## port_letter |= kPinMask;  \
      (initial_value) ? setHigh() : setLow(); \
    } \
  }  
  

namespace lin_processor {

  class Config {
   public:
#if F_CPU != 16000000
#error "The existing code assumes 16Mhz CPU clk."
#endif
    // Initialized to given baud rate. 
    void setup() {
      // If baud rate out of range use default speed.
      uint16 baud = custom_defs::kLinSpeed; 
      if (baud < 1000 || baud > 20000) {
        sio::println(F("ERROR: kLinSpeed out of range"));
        baud = kDefaultBaud;
      }
      baud_ = baud; 
      prescaler_x64_ = baud < 8000;
      const uint8 prescaling = prescaler_x64_ ? 64 : 8;  
      counts_per_bit_ = (((16000000L / prescaling) / baud));
      // Adding two counts to compensate for software delay.
      counts_per_half_bit_ = (counts_per_bit_ / 2) + 2;
      clock_ticks_per_bit_ = (hardware_clock::kTicksPerMilli * 1000) / baud;
      clock_ticks_per_half_bit_ = clock_ticks_per_bit_ / 2;
      clock_ticks_per_until_start_bit_ = clock_ticks_per_bit_ * kMaxSpaceBits;
    }

    inline uint16 baud() const { 
      return baud_; 
    }

    inline boolean prescaler_x64() {
      return prescaler_x64_;
    }

    inline uint8 counts_per_bit() const { 
      return counts_per_bit_; 
    }
    inline uint8 counts_per_half_bit() const { 
      return counts_per_half_bit_; 
    }
    inline uint8 clock_ticks_per_bit() const { 
      return clock_ticks_per_bit_; 
    }
    inline uint8 clock_ticks_per_half_bit() const { 
      return clock_ticks_per_half_bit_; 
    }
    inline uint8 clock_ticks_per_until_start_bit() const { 
      return clock_ticks_per_until_start_bit_; 
    }
   private:
    uint16 baud_;
    // False -> x8, true -> x64.
    // TODO: timer2 also have x32 scalingl Could use it for better 
    // accuracy in the mid baude range.
    boolean prescaler_x64_;
    uint8 counts_per_bit_;
    uint8 counts_per_half_bit_;
    uint8 clock_ticks_per_bit_;
    uint8 clock_ticks_per_half_bit_;
    uint8 clock_ticks_per_until_start_bit_;
  };

  // The actual configurtion. Initialized in setup() based on baud rate.  
  Config config;

  // ----- Digital I/O pins
  //
  // NOTE: we use direct register access instead the abstractions in io_pins.h. 
  // This way we shave a few cycles from the ISR.
    
  // Master LIN interface.
  DEFINE_INPUT_PIN(rx1_pin, D, 2);
  DEFINE_OUTPUT_PIN(tx1_pin, C, 2, 1);
  
  // Slave LIN interface.  
  DEFINE_INPUT_PIN(rx2_pin, C, 1);
  DEFINE_OUTPUT_PIN(tx2_pin, D, 4, 1);
  
  // Debugging signals.
  DEFINE_OUTPUT_PIN(break_pin, C, 0, 0);
  DEFINE_OUTPUT_PIN(sample_pin, B, 4, 0);
  DEFINE_OUTPUT_PIN(error_pin, B, 3, 0);
  DEFINE_OUTPUT_PIN(isr_pin, C, 3, 0);
  DEFINE_OUTPUT_PIN(gp_pin, D, 6, 0);

  // Called one during initialization.
  static inline void setupPins() {
    rx1_pin::setup();
    tx1_pin::setup();
    rx2_pin::setup();
    tx2_pin::setup();
    break_pin::setup();
    sample_pin::setup();
    error_pin::setup();
    isr_pin::setup();
    gp_pin::setup();
  }

  // ----- ISR RX Ring Buffers -----

  // Frame buffer queue size.
  static const uint8 kMaxFrameBuffers = 8;

  // RX Frame buffers queue. Read/Writen by ISR only. 
  static LinFrame rx_frame_buffers[kMaxFrameBuffers];

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
  }

  // Called from ISR or from main with interrupts disabled.
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

  // ----- ISR To Main Data Transfer -----

  // Increment by the ISR to indicates to the main program when the ISR returned.
  // This is used to defered disabling interrupts until the ISR completes to 
  // reduce ISR jitter time.
  static volatile uint8 isr_marker;
  
  // Should be called from main only. 
  static inline void waitForIsrEnd() {
    const uint8 value = isr_marker;
    // Wait until the next ISR ends.
    while (value == isr_marker) {
    } 
  }
  
  // Public. Called from main. See .h for description.
  boolean readNextFrame(LinFrame* buffer) {
    boolean result = false;
    waitForIsrEnd();
    cli();
    if (tail_frame_buffer != head_frame_buffer) {
      //led::setHigh();
      // This copies the request buffer struct.
      *buffer = rx_frame_buffers[tail_frame_buffer];
      incrementTailFrameBuffer();
      result = true;
      //led::setLow();
    }
    sei();
    return result; 
  }

  // ----- State Machine Declaration -----

  // Like enum but 8 bits only.
  namespace states {
    static const uint8 DETECT_BREAK = 1;
    static const uint8 READ_DATA = 2;
  }
  static uint8 state;

  class StateDetectBreak {
   public:
    static inline void enter() ;
    static inline void handleIsr();
    
   private:
    static uint8 low_bits_counter_;
  };

  class StateReadData {
   public:
    // Should be called after the break stop bit was detected.
    static inline void enter();
    static inline void handleIsr();
    
   private:
    // Indicates if we read bytes from master (true) or slave (false).
    static boolean rx_from_lin1_;
    
    // Indicates the transfer function of the next rx bit. One of rx_bit_transfer_functions
    // values.
    static byte rx_bit_transfer_function_;
    
    // Number of complete bytes read so far. Includes all bytes: sync, id,
    // data and checksum.
    static uint8 bytes_read_;
    
    // Number of bits read so far in the current byte. Includes start bit, 
    // 8 data bits and one stop bits. 
    static uint8 bits_read_in_byte_;
    
    // Buffer for the current proxied byte. This byte includes any signal 
    // injection done on this frame.
    static uint8 byte_buffer_;
    
    // When collecting the data bits, this goes (1 << 0) to (1 << 7). Could
    // be computed as (1 << (bits_read_in_byte_ - 1)). We use this cached value
    // recude ISR computation.
    static uint8 byte_buffer_bit_mask_;
    
    // When true, the byte buffer has at least one injected bit. That is, a bit that 
    // was forced to 1 or 0 by the injector, regardless of the original bit value.
    static boolean byte_buffer_has_injected_bits_;
        
    static inline boolean proxyRxBit();
    static inline uint8 nextBitFunction();
  };

  // ----- Error Flag. -----

  // Written from ISR. Read/Write from main.
  static volatile uint8 error_flags;

  // Private. Called from ISR and from setup (beofe starting the ISR).
  static inline void setErrorFlags(uint8 flags) {
    error_pin::setHigh();
    // Non atomic when called from setup() but should be fine since ISR is not running yet.
    error_flags |= flags;
    error_pin::setLow();
  }

  // Called from main. Public. Assumed interrupts are enabled. 
  // Do not call from ISR.
  uint8 getAndClearErrorFlags() {
    // Disabling interrupts for a brief for atomicity. Need to pay attention to
    // ISR jitter due to disabled interrupts.
    cli();
    const uint8 result = error_flags;
    error_flags = 0;
    sei();
    return result;
  }

  struct BitName {
    const uint8 mask;
    const char* const name;  
  };

  static const  BitName kErrorBitNames[] PROGMEM = {
    { errors::FRAME_TOO_SHORT, "SHRT" },
    { errors::FRAME_TOO_LONG, "LONG" },
    { errors::START_BIT, "STRT" },
    { errors::STOP_BIT, "STOP" },
    { errors::SYNC_BYTE, "SYNC" },
    { errors::BUFFER_OVERRUN, "OVRN" },
    { errors::OTHER, "OTHR" },
  };

  // Given a byte with lin processor error bitset, print the list
  // of set errors.
  void printErrorFlags(uint8 lin_errors) {
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

  // ----- Initialization -----

  static void setupTimer() {    
    // OC2B cycle pulse (Arduino digital pin 3, PD3). For debugging.
    DDRD |= H(DDD3);
    // Fast PWM mode, OC2B output active high.
    TCCR2A = L(COM2A1) | L(COM2A0) | H(COM2B1) | H(COM2B0) | H(WGM21) | H(WGM20);
    const uint8 prescaler = config.prescaler_x64()
      ? (H(CS22) | L(CS21) | L(CS20))   // x64
        : (L(CS22) | H(CS21) | L(CS20));  // x8
    // Prescaler: X8. Should match the definition of kPreScaler;
    TCCR2B = L(FOC2A) | L(FOC2B) | H(WGM22) | prescaler;
    // Clear counter.
    TCNT2 = 0;
    // Determines baud rate.
    OCR2A = config.counts_per_bit() - 1;
    // A short 8 clocks pulse on OC2B at the end of each cycle,
    // just before triggering the ISR.
    OCR2B = config.counts_per_bit() - 2; 
    // Interrupt on A match.
    TIMSK2 = L(OCIE2B) | H(OCIE2A) | L(TOIE2);
    // Clear pending Compare A interrupts.
    TIFR2 = L(OCF2B) | H(OCF2A) | L(TOV2);
  }

  // Call once from main at the begining of the program.
  void setup() {
    // Should be done first since some of the steps below depends on it.
    config.setup();

    setupPins();
    setupBuffers();
    StateDetectBreak::enter();
    setupTimer();
    error_flags = 0;

    sio::waitUntilFlushed();
    // TODO: move this to config class.
    sio::printf(F("LIN: %u, %u, %u, %u, %u, %u, %u, %u\n"), 
        config.baud(), 
        custom_defs::kUseLinChecksumVersion2,
        config.prescaler_x64(),
        config.counts_per_bit(), 
        config.counts_per_half_bit(), 
        config.clock_ticks_per_bit(),  
        config.clock_ticks_per_half_bit(),  
        config.clock_ticks_per_until_start_bit());
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
    TCNT2 = config.counts_per_half_bit();
  }
  
  // Perform a tight busy loop for a given number of clock ticks.
  // Keeps the lin tick timer reset.
  // Called from ISR only.
  static inline void wait(uint16 clock_ticks) {
    const uint16 base_clock = hardware_clock::ticksForIsr();
    for(;;) {
      // Keep the tick timer not ticking (no ISR).
      resetTickTimer();

      // Should work also in case of 16 bit clock overflow.
      const uint16 clock_diff = hardware_clock::ticksForIsr() - base_clock;
      if (clock_diff >= clock_ticks) {
        return; 
      }
    } 
  }
  
  static inline boolean isRxHigh(boolean use_rx1) {
    return use_rx1 ? rx1_pin::isHigh() : rx2_pin::isHigh(); 
  } 

  // Perform a tight busy loop until RX is low or the given number
  // of clock ticks passed (timeout). Retuns true if ok,
  // false if timeout. Keeps timer reset during the wait.
  // Called from ISR only.
  static inline boolean waitForRxLow(uint16 max_clock_ticks, boolean use_rx1) {
    const uint16 base_clock = hardware_clock::ticksForIsr();
    for(;;) {
      // Keep the tick timer not ticking (no ISR).
      resetTickTimer();

      // If selected rx is low we are done.
      if (!isRxHigh(use_rx1)) {
        return true;
      }

      // Test for timeout.
      // Should work also in case of 16 bit clock overflow.
      const uint16 clock_diff = hardware_clock::ticksForIsr() - base_clock;
      if (clock_diff >= max_clock_ticks) {
        return false; 
      }
    } 
  }

  // Same as waitForRxLow but with reversed polarity.
  // We clone to code for time optimization.
  // Called from ISR only.
  static inline boolean waitForRxHigh(uint16 max_clock_ticks, boolean use_rx1) {
    const uint16 base_clock = hardware_clock::ticksForIsr();
    for(;;) {
      resetTickTimer();
      if (isRxHigh(use_rx1)) {
        return true;
      }
      // Should work also in case of an clock overflow.
      const uint16 clock_diff = hardware_clock::ticksForIsr() - base_clock;
      if (clock_diff >= max_clock_ticks) {
        return false; 
      }
    } 
  }


  // Wait for a high to low transition in any of the two channels or timeout
  // if max clock ticks elapsed. Return 0 if timeout, 1 if rx1 or 2 if rx2.
  // Keeps the tick timer reset to avoid interrupts.
  // Called from an ISR.
  static inline byte waitForResponseStartBit(uint16 max_clock_ticks) {
    const uint16 base_clock = hardware_clock::ticksForIsr();
    boolean state1 = rx1_pin::isHigh();
    boolean state2 = rx2_pin::isHigh();
    for(;;) {
      resetTickTimer();  
      // Look for high to low transition on channel 1.    
      {
        const boolean new_state1 = rx1_pin::isHigh();
        if (state1 && !new_state1) {
          return 1;
        }
        state1 = new_state1;
      }
      // Look for high to low transition on channel 2.   
      {
        const boolean new_state2 = rx2_pin::isHigh();
        if (state2 && !new_state2) {
          return 2;
        }
        state2 = new_state2;
      }
      
      // Check for timeout.
      // NOTE: substraction should work also in case of an clock overflow.
      const uint16 clock_diff = hardware_clock::ticksForIsr() - base_clock;
      if (clock_diff >= max_clock_ticks) {
        return 0; 
      }
    } 
  }
  
  // ----- Detect-Break State Implementation -----

  uint8 StateDetectBreak::low_bits_counter_;

  inline void StateDetectBreak::enter() {
    state = states::DETECT_BREAK;
    low_bits_counter_ = 0;
    // Make sure we don't assert a break on the lin1 bus.
    tx1_pin::setHigh();
    // Make slave TX output passive.
    tx2_pin::setHigh();
  }

  // Return true if enough time to service rx request.
  inline void StateDetectBreak::handleIsr() {
    if (rx1_pin::isHigh()) {
      tx2_pin::setHigh();
      low_bits_counter_ = 0;
      return;
    } 

    // Here RX is low (active)  
    // TODO: since the slave is delayed by 1/2 bit, will be nice to delay also
    // the begining of the break.
    tx2_pin::setLow();

    if (++low_bits_counter_ < 10) {
      return;
    }

    // Detected a break. Wait for rx high and enter data reading.
    break_pin::setHigh();

    // TODO: set actual max count
    waitForRxHigh(255, true);
    break_pin::setLow();

    // Wait for half a bit before we propogate the end of the break
    // to the slave. The slave is delayed by half a bit.
    wait(config.clock_ticks_per_half_bit());
    tx2_pin::setHigh();
   
    // Go process the data
    StateReadData::enter();
  }

  // ----- Read-Data State Implementation -----

  uint8 StateReadData::bytes_read_;
  uint8 StateReadData::bits_read_in_byte_;
  boolean StateReadData::rx_from_lin1_;
  byte StateReadData::rx_bit_transfer_function_;
  uint8 StateReadData::byte_buffer_;
  uint8 StateReadData::byte_buffer_bit_mask_;
  boolean StateReadData::byte_buffer_has_injected_bits_;

  // Called after half a bit after the low to high transition at the end of the break.
  inline void StateReadData::enter() {
    state = states::READ_DATA;
    bytes_read_ = 0;
    bits_read_in_byte_ = 0;
    rx_frame_buffers[head_frame_buffer].reset();
    
    // True = reading from master, sending to slave.
    rx_from_lin1_ = true;
    rx_bit_transfer_function_ = injector_actions::COPY_BIT;

    // TODO: handle post break timeout errors.
    // TODO: set a reasonable time limit.
    waitForRxLow(255, true);
    setTimerToHalfTick();   
  }

  // Called from ISR. Read an rx bit and transfer to the other interface with
  // possible transformation. Uses rx_from_lin1_ to determine direction and uses
  // rx_bit_transfer_function to determine bit transformation function.
  // Returns the read bit post transformation.
  //
  // TODO: currently when forcing a 1 or 0 bit, we completely ignore the input
  // bit. Ideally we should read it, verify the checksum of the incoming frame
  // and if invalid (e.g. due to electrical noise), corrupt the checksum of 
  // the modify frame. Otherwise we may laundering bad bits when recaclulating
  // the checksum for the transformed frame.
  inline boolean StateReadData::proxyRxBit() {
    sample_pin::setHigh();

    // Represent proxied bit (after transformation).
    boolean is_rx_high = true;  
    
    if (rx_from_lin1_) {
      // Master interface to slave interface transfer.
      switch (rx_bit_transfer_function_) {
        case injector_actions::COPY_BIT:
          is_rx_high = rx1_pin::isHigh();
          if (is_rx_high) {
            tx2_pin::setHigh();
          } else {
            tx2_pin::setLow();
          }
          break;

        case injector_actions::FORCE_BIT_1:
          is_rx_high = true;
          tx2_pin::setHigh();
          break;
  
        case injector_actions::FORCE_BIT_0:
          is_rx_high = false;
          tx2_pin::setLow();
          break;

        default:
          // TODO: set an error flag.
          // For errors, we use the passive state of the LIN bus.
          is_rx_high = true;
          break;
      }
    } else {
      // Slave interface to master interface transfer.
      switch (rx_bit_transfer_function_) {
        case injector_actions::COPY_BIT:
          is_rx_high = rx2_pin::isHigh();
          if (is_rx_high) {
            tx1_pin::setHigh();
          } else {
            tx1_pin::setLow();
          }
          break;

        case injector_actions::FORCE_BIT_1:
          is_rx_high = true;
          tx1_pin::setHigh();
          break;
  
        case injector_actions::FORCE_BIT_0:
          is_rx_high = false;
          tx1_pin::setLow();
          break;

        default:
          // TODO: set an error flag.
          // For errors, we use the passive state of the LIN bus.
          is_rx_high = true;
          break;
      }
    }
    
    sample_pin::setLow();
    return is_rx_high;
  }

  // Returns the transfer function of the next bit.
  inline uint8 StateReadData::nextBitFunction() {
    // Force copy if sync byte, id byte, any start bit or any stop bit.
    if (bytes_read_ < 2 || bits_read_in_byte_ == 0 || bits_read_in_byte_ >= 9) {
      return injector_actions::COPY_BIT;
    }
    
    // First data bit is <0, 0>
    // Ignore the sync and id bytes and all the start bits.
    return custom_injector::onIsrNextBitAction(bytes_read_ - 2, bits_read_in_byte_ - 1);  
  }
  
  inline void StateReadData::handleIsr() {
    // Sample and propogated the data bit ASAP to avoid jitter.
    // Since we sample at the middle of the input bit, the output
    // channel is delayed by 1/2 bit.
    const boolean is_rx_high = proxyRxBit(); 
    
    // Handle byte's start bit.
    if (bits_read_in_byte_ == 0) {
      // Start bit error.
      if (is_rx_high) {
        // If in sync byte, report as a sync error.
        setErrorFlags(bytes_read_ == 0 ? errors::SYNC_BYTE : errors::START_BIT);
        StateDetectBreak::enter();
        // No need to set next bit action, we exit the data reading state.
        return;
      }  
      
      // Here when start bit is ok.
      bits_read_in_byte_++;
      // Prepare buffer and mask for data bit collection.
      byte_buffer_ = 0;
      byte_buffer_bit_mask_ = (1 << 0);
      byte_buffer_has_injected_bits_ = false;
      rx_bit_transfer_function_ = nextBitFunction();
      return;
    }

    // If this is one of the 8 data bits handle it. This does not handle the stop bit.
    // Collect the current bit into byte_buffer_, lsb first.
    if (bits_read_in_byte_ <= 8) {
      if (is_rx_high) {
        byte_buffer_ |= byte_buffer_bit_mask_;
      }
      byte_buffer_bit_mask_ = byte_buffer_bit_mask_ << 1;
      bits_read_in_byte_++;
      // If that bit value was forced by the injector, mark this byte as having injected bits.
      if (rx_bit_transfer_function_ != injector_actions::COPY_BIT) {
        byte_buffer_has_injected_bits_ = true;
      }
      // Set the transfer function for the next rx bit.
      rx_bit_transfer_function_ = nextBitFunction();
      return;
    }
    
    // Here when in a stop bit.
    bytes_read_++;
    bits_read_in_byte_ = 0;

    // Error if stop bit is not high.
    if (!is_rx_high) {
      // If in sync byte, report as sync error.
      setErrorFlags(bytes_read_ == 0 ? errors::SYNC_BYTE : errors::STOP_BIT);
      StateDetectBreak::enter();
      // No need to set bit function, we exit the data reading state.
      return;
    }  
    
    // Here when we just finished reading a byte. 
    // bytes_read is already incremented for this byte.
    
    // If this is the sync byte, verify that it has the expected value.
    if (bytes_read_ == 1) {
      // Should be exactly 0x55. We don't append this byte to the buffer.
      if (byte_buffer_ != 0x55) {
        setErrorFlags(errors::SYNC_BYTE);
        StateDetectBreak::enter();
        // No need to set the next bit function, we exit the data reading state.
        return;
      }
      // We do not append the sync byte to the buffer.
    } else {
      // If this is the id, data or checksum bytes, append it to the frame buffer.
      // NOTE: the byte limit count is enforeced somewhere else so we can assume safely here that this 
      // will not cause a buffer overlow.
      rx_frame_buffers[head_frame_buffer].append_byte(byte_buffer_, byte_buffer_has_injected_bits_);    
    }
    
    // Report data and checksum bytes, skipping the sync and frame id bytes.
    if (bytes_read_ >= 3) {     
      // TODO: call this after the 8th data bit, before the stop bit, this way we will have
      // a full bit slot to comptue the checksum rather than half a bit, until the high to low
      // transition of the next start bit.
      custom_injector::onIsrByteSent(bytes_read_ - 3, byte_buffer_);
    }
        
    // Determine if there are more bytes.
    boolean has_more_bytes = false;
    if (bytes_read_ == 2) {  
      // This is the case where we just read the id byte from the master.
      // Inform the injector.      
      custom_injector::onIsrFrameIdRecieved(byte_buffer_);
            
      // Master sent sync and ID bytes and now we need to wait for the response. It can 
      // come from the master (1) or the slave (2), or 0 for timeout.
      // TODO: user longer timeout than for normal bytes.
      const byte lin_channel_ = waitForResponseStartBit(config.clock_ticks_per_until_start_bit());
      has_more_bytes = lin_channel_;
      rx_from_lin1_ = (lin_channel_ != 2);
    } else {
      // This is the case where we don't need to check where the next byte is comming 
      // from.
      //
      // Wait for the high to low transition of start bit of next byte. Using existing channel.
      has_more_bytes =  waitForRxLow(config.clock_ticks_per_until_start_bit(), rx_from_lin1_);
    }

    // Handle the case of no more bytes in this frame.
    if (!has_more_bytes) {
      // Verify min byte count.
      if (bytes_read_ < LinFrame::kMinBytes) {
        setErrorFlags(errors::FRAME_TOO_SHORT);
        StateDetectBreak::enter();
        // No need to set the next bit function, we exit the data reading state.
        return;
      }

      // Frame looks ok so far. Move to next frame in the ring buffer.
      // NOTE: we will reset the byte_count of the new frame buffer next time we will enter data detect state.
      // NOTE: verification of sync byte, id, checksum, etc is done latter by the main code, not the ISR.
      incrementHeadFrameBuffer();
      if (tail_frame_buffer == head_frame_buffer) {
        // Frame buffer overrun. We drop the oldest frame and continue with this one.       
        setErrorFlags(errors::BUFFER_OVERRUN);
        incrementTailFrameBuffer();
      }

      StateDetectBreak::enter();
      // No need to set the next bit function, we exit the data reading state.
      return;
    }

    // Here when there is at least one more byte in this frame. Error if we already had
    // the max number of bytes.
    if (rx_frame_buffers[head_frame_buffer].num_bytes() >= LinFrame::kMaxBytes) {
      setErrorFlags(errors::FRAME_TOO_LONG);
      StateDetectBreak::enter();
      // No need to set the next bit function, we exit the data reading state.
      return;  
    }

    // Everything is ready for the next byte. Have a tick in the middle of its
    // start bit, alsywas with a copy function.
    //
    //    
    // TODO: move this to above the num_bytes check above for more accurate 
    // timing of mid bit tick? 
    rx_bit_transfer_function_ = injector_actions::COPY_BIT;
    setTimerToHalfTick();
  }

  // ----- ISR Handler -----

  // Interrupt on Timer 2 A-match.
  ISR(TIMER2_COMPA_vect)
  {
    isr_pin::setHigh();
    // TODO: make this state a boolean instead of enum? (efficency).
    switch (state) {
    case states::DETECT_BREAK:
      StateDetectBreak::handleIsr();
      break;
    case states::READ_DATA:
      StateReadData::handleIsr();
      break;
    default:
      setErrorFlags(errors::OTHER);
      StateDetectBreak::enter();
    }

    // Increment the isr flag to indicate to the main that the ISR just
    // exited and interrupts can be temporarily disabled without causes ISR 
    // jitter.
    isr_marker++;
    
    isr_pin::setLow();
  }
}  // namespace lin_processor

