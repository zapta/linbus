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

#ifndef LIN_DECODER_H
#define LIN_DECODER_H

#include <arduino.h>
#include "avr_util.h"

// Uses Timer2 with interrupts, PB2, PD2, PD3, PD4.
namespace lin_decoder {
  // A buffer for a single recieved frame.
  typedef struct RxFrameBuffer {
    // 1 ID byte + up to 8 data bytes + 1 checksum byte. 
    static const uint8 kMaxBytes = 10;
    // Number of bytes in bytes[].
    uint8 num_bytes;
    // Recieved bytes. Includes id, data and checksum but not sync.
    uint8 bytes[kMaxBytes];
  } RxFrameBuffer;
  
  // Call once in program setup. 
  extern void init();
  
  // Try to read next available rx frame. If available, return true and set
  // given buffer. Otherwise, return false and buffer content is unspecified. 
  extern boolean readNextFrame(RxFrameBuffer* buffer);
  
  // Error status.
  extern boolean hasErrors();
  extern void clearErrors();
}

#endif  

