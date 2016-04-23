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

#include <avr/eeprom.h>

#include "custom_module.h"

#include "custom_config.h"
#include "custom_injector.h"
#include "custom_signals.h"
#include "io_pins.h"
#include "leds.h"
#include "signal_tracker.h"
#include "sio.h"

// Like all the other custom_* files, this file should be adapted to the specific application. 
// The example provided is for a Sport/PSE button memory feature for 981 Boxster/Cayman 
// (probably compatible with 991 models as well)
namespace custom_module {

// (0 and 1 are the 16-bit word for config enable/disable)
uint8_t *EEPROM_SPORT_BYTE_ADDR = (uint8_t *) 2;
uint8_t *EEPROM_PSE_BYTE_ADDR   = (uint8_t *) 3;
uint8_t *EEPROM_ASS_BYTE_ADDR   = (uint8_t *) 4;
  
namespace states {
  static const uint8 WAIT_IGNITION = 0;
  static const uint8 POLL          = 1;
  static const uint8 INJECT_SPORT  = 2;
  static const uint8 INJECT_PSE    = 3;
  static const uint8 INJECT_ASS    = 4;
}

// The current state. One of states:: values. 
static uint8 state;

// Tracks since change to current state.
static PassiveTimer time_in_state;
  
static inline void changeToState(uint8 new_state) {
  state = new_state;
  // We assume this is a new state and always reset the time in state.
  time_in_state.restart();
}

void setup() {
  custom_signals::setup();
  custom_config::setup();
  changeToState(states::WAIT_IGNITION);
}
  
static inline void updateState() 
{
   //
   // Button control FSM
   //
   // - Inhibit further processing if memory function disabled or ignition off
   //
   // - If Sport LED state disagrees with EEPROM, see if the physical button is down.  If so, update the EEPROM
   //   to store the user's new setting.  Otherwise, if Sport Plus isn't enabled, inject a Sport button press
   //   for 500 ms.  (We don't want to cancel a user-initiated transition from Sport to Sport Plus mode.)
   //
   // - If PSE LED state disagrees with EEPROM, see if the physical button is down.  If so, update the EEPROM
   //   to store the user's new setting.  Otherwise, inject a PSE button press for 500 ms
   //
   // - A button is considered to be pressed by the user if it is either down ("on") when polled, or has 
   //   been off for less than 250 milliseconds.  This lag keeps us from reverting quick button presses that
   //   are released by the user before we see the corresponding event on the LIN bus
   //

   if (custom_signals::ignition_state().isOff())
      {
      changeToState(states::WAIT_IGNITION);
      }

   switch (state)
      {
      case states::WAIT_IGNITION:
         {
         custom_injector::disableSportInject();
         custom_injector::disablePSEInject();
         custom_injector::disableASSInject();

         if (custom_config::is_enabled() && custom_signals::ignition_state().isOnForAtLeastMillis(1000))
            {
            changeToState(states::POLL);
            }

         break;
         }

      case states::POLL:
         {
         boolean sport_plus_active = custom_signals::sport_plus_LED().isOn();
         boolean sport_remembered  = eeprom_read_byte(EEPROM_SPORT_BYTE_ADDR);
         boolean sport_active      = custom_signals::sport_LED().isOn();
         boolean sport_button_down = custom_signals::sport_switch().isOn() || (custom_signals::sport_switch().timeInStateMillis() < 250);

         if (sport_active != sport_remembered)
            {
            if (sport_button_down)
               {
               eeprom_write_byte(EEPROM_SPORT_BYTE_ADDR, sport_active);
               }
            else
               {
               if (!sport_plus_active)
                  {
                  sio::printf(F("Sport inject\n"));
                  custom_injector::setSportInject(true);
                  changeToState(states::INJECT_SPORT);
                  break;
                  }
               }
            }

         boolean PSE_remembered  = eeprom_read_byte(EEPROM_PSE_BYTE_ADDR);
         boolean PSE_active      = custom_signals::PSE_LED().isOn();
         boolean PSE_button_down = custom_signals::PSE_switch().isOn() || (custom_signals::PSE_switch().timeInStateMillis() < 250); 

         if (PSE_active != PSE_remembered)
            {
            if (PSE_button_down)
               {
               eeprom_write_byte(EEPROM_PSE_BYTE_ADDR, PSE_active);
               }
            else
               {
               sio::printf(F("PSE inject\n"));
               custom_injector::setPSEInject(true);
               changeToState(states::INJECT_PSE);
               break;
               }
            }

         boolean ASS_remembered  = eeprom_read_byte(EEPROM_ASS_BYTE_ADDR);
         boolean ASS_active      = custom_signals::autostart_LED().isOn();
         boolean ASS_button_down = custom_signals::autostart_switch().isOn() || (custom_signals::autostart_switch().timeInStateMillis() < 250); 

         if (ASS_active != ASS_remembered)
            {
            if (ASS_button_down)
               {
               eeprom_write_byte(EEPROM_ASS_BYTE_ADDR, ASS_active);
               }
            else
               {
               sio::printf(F("ASS inject\n"));
               custom_injector::setASSInject(true);
               changeToState(states::INJECT_ASS);
               break;
               }
            }

         break;
         }

      case states::INJECT_SPORT:
         {
         uint32 btime = time_in_state.timeMillis();

         if (btime > 500)
            {
            sio::printf(F("Sport release after %d ms\n"), btime);
            custom_injector::disableSportInject();
            changeToState(states::POLL);
            }

         break;
         }

      case states::INJECT_PSE:
         {
         uint32 btime = time_in_state.timeMillis();

         if (btime > 500)
            {
            sio::printf(F("PSE release after %d ms\n"), btime);
            custom_injector::disablePSEInject();
            changeToState(states::POLL);
            }

         break;
         }

      case states::INJECT_ASS:
         {
         uint32 btime = time_in_state.timeMillis();

         if (btime > 500)
            {
            sio::printf(F("ASS release after %d ms\n"), btime);
            custom_injector::disableASSInject();
            changeToState(states::POLL);
            }

         break;
         }

      default:
         {
         sio::printf(F("BS: unknown state (%d)"), state);
         changeToState(states::WAIT_IGNITION);
         break;
         }
      }
}
 
static inline void showPanelState()
{
   sio::printf(F("AS=%d/%d PASM=%d/%d PSE=%d/%d PSM=%d/%d RC=%d RO=%d Spoil=%d/%d S=%d/%d SP=%d/%d\n"),
      custom_signals::autostart_switch().isOn(),
      custom_signals::autostart_LED().isOn(),
      custom_signals::PASM_switch().isOn(),
      custom_signals::PASM_LED().isOn(),
      custom_signals::PSE_switch().isOn(),
      custom_signals::PSE_LED().isOn(),
      custom_signals::PSM_switch().isOn(),
      custom_signals::PSM_LED().isOn(),
      custom_signals::roof_close_switch().isOn(),
      custom_signals::roof_open_switch().isOn(),
      custom_signals::spoiler_switch().isOn(),
      custom_signals::spoiler_LED().isOn(),
      custom_signals::sport_switch().isOn(),
      custom_signals::sport_LED().isOn(),
      custom_signals::sport_plus_switch().isOn(),
      custom_signals::sport_plus_LED().isOn());
}

void loop() {
  // Update dependents.
  custom_signals::loop();
  custom_config::loop();
  
#if 1
   // Update the state machine
   updateState();
#else
   // Diagnostic mode: print button and LED states
   showPanelState();
#endif

   sio::waitUntilFlushed();
}

void frameArrived(const LinFrame& frame) {
  // Track the signals in this frame.
  custom_signals::frameArrived(frame);
  
  // Report an error if the Sport Mode assembly does not respond as expected
  // to its frame.
  if (frame.get_byte(0) == 0x8e) {
    if (frame.num_bytes() != (1 + 8 + 1)) {
      leds::errors.action();
      sio::println(F("slave error")); 
    }
  }
}

}  // namespace custom_module


