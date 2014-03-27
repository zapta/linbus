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

#include "custom_injector.h"

// The state variables of the injector.
//
// Like all the other custom_* files, thsi file should be adapted to the specific application. 
// The example provided is for a Sport Mode button press injector for 981/Cayman.
namespace custom_injector {
  
  // Private state of the injector. Do not use from other files.
  namespace private_ {
    boolean injections_enabled = false;
    
    // True if the current linbus frame is transformed by the injector. Othrwise, the 
    // frame is passed as is.
    boolean frame_id_matches;
    
    // Used to calculate the modified frame checksum.
    uint16 sum;
    
    // The modified frame checksum byte. 
    uint8 checksum;
  }
}  // namepsace custom_injector



