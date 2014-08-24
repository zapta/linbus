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

#ifndef INJECTOR_ACTIONS_H
#define INJECTOR_ACTIONS_H

// A 8 bit enum with transfer functions of proxyied bits.
namespace injector_actions {
  // Transfer the bit as is.
  static const uint8 COPY_BIT = 0;
  
  // Force the output bit to be 1, regardless of the input bit.
  static const uint8 FORCE_BIT_1 = 1;
  
  // Force the output bit to be 0, regardless of the input bit.
  static const uint8 FORCE_BIT_0 = 2;
}  // namespace injector_actions

#endif


