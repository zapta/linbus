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

#include "car_module.h"
#include "car_module_config.h"
#include "action_buzzer.h"
#include "action_led.h"
#include "sio.h"

namespace car_module {
  
// FRAMES LED - blinks when detecting valid frames.
static ActionLed frame_activity_led(PORTB, 0);
  
// Used to generate slow blinking to show the board is live, even when
// there are no frames.
static PassiveTimer idle_timer;

void setup() {
  action_buzzer::setup();
  
  // Force a startup single blink. 
  frame_activity_led.action();
}
  
void loop() {
  action_buzzer::loop();
  frame_activity_led.loop();
  
  if (idle_timer.timeMillis() >= 1000) {
    frame_activity_led.action();
    idle_timer.restart();
  }
}
  
void frameArrived(const LinFrame& frame) {
  frame_activity_led.action();

  // We handle only framed of id 39.
  const uint8 id = frame.get_byte(0);
  if (id != 0x39) {
    return;
  }

  // We expect a frame with one ID byte, 6 data bytes and one checksum byte.
  if (frame.num_bytes()  != (1 + 6 + 1)) {
    return;
  }
  
  // Specific logic for P981/CS linbus of the homelink console. Triggers the
  // buzzer when rear gear is engaged.
  // Test frame: 39 04 00 00 00 00 00.
  const boolean reverse_gear = frame.get_byte(1) & H(2);
  const boolean feature_enabled = car_module_config::isEnabled();
  action_buzzer::action(reverse_gear && feature_enabled); 
 
  // Tell the config module if to allow config changes via long
  // presses. We allow this only when in reverse gear to allow
  // better feedback via the buzzer and to eliminate the chance of
  // unintended config changes.:w
  car_module_config::allowConfigChanges(reverse_gear); 
}

}  // namespace car_module


