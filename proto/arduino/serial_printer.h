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

#ifndef SERIAL_PRINTER_H
#define SERIAL_PRINTER_H

#include <arduino.h>
#include <print.h>
#include "avr_util.h"

// A serial output that uses hardware UART0 but with no interrupts.
// Requires periodic calls to update() to send buffered bytes to
// the uart. Inherits methods like print and println from Print.
// The class exports a singleton instance that is aliased by
// the user friendly name SerialPrintr.
class _SerialPrinter : 
public Print {
public:
  ~_SerialPrinter() {
  }
  
  // The singleton instance.
  static _SerialPrinter instance;
  
  void setup() {
    _start = 0;
    _count = 0;
    
    // 115.2k baud @ 16MHz, per table 20.7.
    UBRR0H = 0;
    UBRR0L = 16;
    UCSR0A = H(U2X0);
    // Enable  the transmitter. Reciever is disabled.
    UCSR0B = H(TXEN0);
    UCSR0C = H(UDORD0) | H(UCPHA0);  //(3 << UCSZ00);  
  }

  // Overrides Print::write().
  size_t write(uint8_t b) {
    if (_count >= kQueueSize) {
      return 0;
    }
    enqueue(b);
    return 1;
  }

  // Called periodically from loop() to do the transmission. Non
  // blocking.
  void loop() {
    if (_count && (UCSR0A & H(UDRE0))) {
      UDR0 = dequeue();
    }
  }
  
  bool is_empty() {
    return !_count;
  }
  
  void printHexByte(uint8 b) {
    printHexDigit(b >> 4);
    printHexDigit(b & 0xf);
  }

private:
  // Max transmit queue size. Should be less than 255/2 to 
  // make sure the byte arithmetic below does not overflow.
  static const byte kQueueSize = 80;
  byte _buffer[kQueueSize];
  
  // Index of first (oldest) byte. [0, kQueueSize).
  byte _start;
  
  // Number of bytes in the queue. [0, kQueueSize].
  byte _count;

  // Private constructor to restrict instantiation to
  // a single instance.
  _SerialPrinter() {  
  }
    
  // Assumes _count < kQueueSize.
  void enqueue(byte b) {
    // kQueueSize is small enough that this will not overflow.
    byte next = _start + _count;
    if (next >= kQueueSize) {
      next -= kQueueSize;
    } 
    _buffer[next] = b;  
    _count++; 
  }

  // Assumes _count > 0.
  byte dequeue() {
    const byte b = _buffer[_start];
    if (++_start >= kQueueSize) {
      _start = 0;
    }
    _count--;
    return b;  
  }
  
  // Assuming n is in [0, 15].
  void printHexDigit(uint8 n) {
    if (n < 10) {
      print((char)('0' + n));
    } else {
      print((char)(('a' - 10) + n));
    }    
  }
};

// Provides a user friendly name for _SerialPrinter::instance;
#define SerialPrinter (_SerialPrinter::instance)

#endif  


