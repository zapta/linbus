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

#include "avr_util.h"
#include "buttons.h"
#include "config.h"
#include "hardware_clock.h"
#include "leds.h"
#include "ltc2943.h"
#include "passive_timer.h"
#include "sio.h"
#include "system_clock.h"

// The value of the shunt resistor, in ohms.
static const float kShuntResistorOhms = 0.025;

namespace consts {
  static const float kLtc2943ChargeLsb = 0.34E-3;
  static const uint16 kChargePrescaler = 1;
  // TODO: define const for 50e-3 (what is it).
  static const float kAvgUaPerChargeTickPerSecond = (( kLtc2943ChargeLsb * kChargePrescaler * 50e-3 * 60 * 60 * 1000000L)/(kShuntResistorOhms * 4096));
  
  // 10 samples per second.
  namespace fast_mode {
    static const uint16 kReportingPeriodMillis = 100;
    static const float kChargeToAvgUaConversion = kAvgUaPerChargeTickPerSecond * 1000 / kReportingPeriodMillis;
  }
  

  // 1 sample per second.
  namespace slow_mode {
    static const uint16 kReportingPeriodMillis = 1000;
    static const float kChargeToAvgUaConversion = kAvgUaPerChargeTickPerSecond * 1000 / kReportingPeriodMillis;
  } 
}

// Represent a measurement mode.
struct Mode {
  const uint16 reporting_time_millis;
  const float charge_ticks_to_avg_ua_conversion;

  Mode(uint16 reporting_time_millis, float charge_ticks_to_avg_ua_conversion)
  : 
   reporting_time_millis(reporting_time_millis),
   charge_ticks_to_avg_ua_conversion(charge_ticks_to_avg_ua_conversion) {
  }
};

// Mode table. Indexed by config::modeIndex();
static const Mode modes[] = {
  Mode(consts::slow_mode::kReportingPeriodMillis, consts::slow_mode::kChargeToAvgUaConversion),
  Mode(consts::fast_mode::kReportingPeriodMillis, consts::fast_mode::kChargeToAvgUaConversion),
};

// 8 bit enum with main states.
namespace states {
  const uint8 INIT = 1;
  const uint8 REPORTING = 3;
  const uint8 ERROR = 4;
}
static uint8 state = 0;

// INIT state declaration.
class StateInit {
  public:
    static inline void enter();
    static inline void loop();  
};

// REPORTING state declaration.
class StateReporting {
  public:
    static inline void enter();
    static inline void loop();  
  private:
    // Index of modes table entry of current mode.
    static uint8 current_mode_index;
    static bool has_last_reading;
    static uint32 last_report_time_millis;
    static uint16 last_report_charge_reading;
};
uint8 StateReporting::current_mode_index;
bool StateReporting::has_last_reading;
uint32 StateReporting::last_report_time_millis;
uint16 StateReporting::last_report_charge_reading;

// ERROR state declaration.
class StateError {
  public:
    static inline void enter();
    static inline void loop(); 
  private:
    static PassiveTimer time_in_state; 
};
PassiveTimer StateError::time_in_state;

// Arduino setup function. Called once during initialization.
void setup()
{
  // Disable timer 0 interrupts. It is setup by the Arduino runtime for the 
  // Arduino time services (which we do not use).
  TCCR0B = 0;
  TCCR0A = 0;

  // Hard coded to 115.2k baud. Uses URART0, no interrupts.
  // Initialize this first since some setup methods uses it.
  sio::setup();
  
  if (config::isDebug()) {
    sio::printf(F("\nStarted\n"));
  } else {
    sio::printf(F("\n"));
  }

  // Uses Timer1, no interrupts.
  hardware_clock::setup();
  
  config::setup();
  
  buttons::setup();
  
  // Let the config value stablizes through the debouncer.
  while (!config::hasStableValue()) {
    system_clock::loop();
    config::loop();
  }
  
  // TODO: disabled unused Arduino time interrupts.


  // Initialize the LTC2943 driver and I2C library.
  // TODO: move this to state machine, check error code.
  ltc2943::setup();
  
  StateInit::enter();
 
  // Enable global interrupts.
  sei(); 
  
  // Have an early 'waiting' led bling to indicate normal operation.
  leds::activity.action(); 
}

//static inline void 

void StateInit::enter() {
  state = states::INIT;
  if (config::isDebug()) {
    sio::printf(F("# State: INIT\n"));
  }
}

void StateInit::loop() {
  if (ltc2943::init()) {
    StateReporting::enter();
  } else {
    if (config::isDebug()) {
      sio::printf(F("# device init failed\n"));
    }
    StateError::enter();
  }
}

void StateReporting::enter() {
  state = states::REPORTING;
  has_last_reading = false;
  // We switch modes only when entering the reporting state. If changed
  // while in the reporting state, we reenter it. This provides a graceful
  // transition.
  current_mode_index = config::modeIndex();
  if (config::isDebug()) {
    sio::printf(F("# Mode: %d\n"), current_mode_index);
    sio::printf(F("# State: REPORTING.0\n"));
  }
}

void StateReporting::loop() {
  // Check for mode change.
  if (config::modeIndex() != current_mode_index) {
     if (config::isDebug()) {
        sio::printf(F("# Mode changed\n"));
      }
      // The switch is done when reentering the state. This provides graceful 
      // transition.
      StateReporting::enter();
    return;
  }
  
  // Try first reading.
  if (!has_last_reading) {
    if (!ltc2943::readAccumCharge(&last_report_charge_reading)) {
      if (config::isDebug()) {
        sio::printf(F("# First reading failed\n"));
      }
      StateError::enter();
      return;
    }
    last_report_time_millis = system_clock::timeMillis();
    has_last_reading = true;
    if (config::isDebug()) {
      sio::printf(F("# State: REPORTING.1\n"));
    }
    return;
  }
  
  // Here when successive reading. If not time yet to next reading do nothing.
  // NOTE: the time check below should handle correctly 52 days wraparound of the uint32
  // time in millis.
  const Mode& current_mode = modes[current_mode_index];
  const uint16 current_reporting_time_millis = current_mode.reporting_time_millis;
  
  const uint32 time_now_millis = system_clock::timeMillis();
  const int32 time_diff_millis = time_now_millis - last_report_time_millis;
  if (time_diff_millis < current_reporting_time_millis) {
    return;
  }
  
  // NOTE: we keedp the nominal reporting rate. Jitter in the reporting time will not 
  // create an accmulating errors in the reporting charge since we map the charge to
  // current using the nominal reporting rate as used by the consumers of this data.
  last_report_time_millis += current_reporting_time_millis;
  
  // Do the successive reading.
  uint16 current_report_charge_reading;
  if (!ltc2943::readAccumCharge(&current_report_charge_reading)) {
      if (config::isDebug()) {
        sio::printf(F("# successive reading failed\n"));
      }
      StateError::enter();
      return;
  }
  
  // NOTE: this should handle correctly charge register wraps around.
  const int16 charge_reading_diff = (current_report_charge_reading - last_report_charge_reading);
  last_report_charge_reading = current_report_charge_reading;
  const int32 avg_micro_amps = (int32)(charge_reading_diff * current_mode.charge_ticks_to_avg_ua_conversion);
  
  // NOTE: since the stock Arduino printf does not support floats, we break the 
  // value into two integer values, the integer and the fraction in ppms.
  const int16 amps = avg_micro_amps / 1000000L;
  const int32 uamps = avg_micro_amps - (amps * 1000000L);
  
  if (config::isDebug()) {
    sio::printf(F("%04x, %d, %d.%06ld\n"), last_report_charge_reading, charge_reading_diff, amps, uamps);
  } else {
    sio::printf(F("%08lu %d.%06ld\n"), time_now_millis, amps, uamps);  
  }
  leds::activity.action(); 
}

inline void StateError::enter() {
  state = states::ERROR;
  if (config::isDebug()) {
    sio::printf(F("# State: ERROR\n"));
  }
  time_in_state.restart();  
  leds::errors.action();
}

inline void StateError::loop() {
  // Insert a short delay to avoid flodding teh serial output with error messages
  // in case we have a permanent error condition.
  if (time_in_state.timeMillis() < 100) {
    return;
  }
  // Try again from scratch.
  StateInit::enter();
}

// Arduino loop() method. Called after setup(). Never returns.
// This is a quick loop that does not use delay() or other 'long' busy loops
// or blocking calls. typical iteration is ~ 50usec with 16Mhz CPU.
void loop()
{
  // Having our own loop shaves about 4 usec per iteration. It also eliminate
  // any underlying functionality that we may not want.
  for(;;) {   
     //leds::debug.high();
 
    // Periodic updates.
    system_clock::loop();  
    config::loop();  
    buttons::loop();
    sio::loop();
    leds::loop(); 
    
    switch (state) {
      case states::INIT:
        StateInit::loop();
        break;
      case states::REPORTING:
        StateReporting::loop();
        break;
      case states::ERROR:
        StateError::loop();
        break;
      default:
        if (config::isDebug()) {
          sio::printf(F("# Unknown state: %d\n"), state);
        }
        StateError::enter();
        break;  
    }
    
   //leds::debug.low();
  }
}


