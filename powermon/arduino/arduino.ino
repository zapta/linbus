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
#include "config.h"
#include "hardware_clock.h"
#include "leds.h"
#include "ltc2943.h"
#include "passive_timer.h"
#include "sio.h"
#include "system_clock.h"

// Report internal in millis.  This does not affect the current integration 
// accuracy (which is a continious analog integrator).
const uint16 kReportingPeriodMillis = 1000;

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
    static bool has_last_reading;
    static uint32 last_report_time_millis;
    static uint16 last_report_charge_reading;
};
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
  // Hard coded to 115.2k baud. Uses URART0, no interrupts.
  // Initialize this first since some setup methods uses it.
  sio::setup();
  
  if (config::isDebug()) {
    sio::printf(F("\nStarted\n"));
  } else {
    sio::println();
  }

  // Uses Timer1, no interrupts.
  hardware_clock::setup();
  
  config::setup();
  
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
  if (config::isDebug()) {
    sio::printf(F("# State: REPORTING.0\n"));
  }
}

void StateReporting::loop() {
  // Try first reading.
  if (!has_last_reading) {
    if (!ltc2943::readReg16(ltc2943::regs16::kAccumCharge, &last_report_charge_reading)) {
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
  const uint32 time_now_millis = system_clock::timeMillis();
  const int32 time_diff_millis = time_now_millis - last_report_time_millis;
  if (time_diff_millis < kReportingPeriodMillis) {
    return;
  }
  
  // NOTE: we keedp the nominal reporting rate. Jitter in the reporting time will not 
  // create an accmulating errors in the reporting charge since we map the charge to
  // current using the nominal reporting rate as used by the consumers of this data.
  last_report_time_millis += kReportingPeriodMillis;
  
  // Do the successive reading.
  uint16 current_report_charge_reading;
  if (!ltc2943::readReg16(ltc2943::regs16::kAccumCharge, &current_report_charge_reading)) {
      if (config::isDebug()) {
        sio::printf(F("# successive reading failed\n"));
      }
      StateError::enter();
      return;
  }
  
  // NOTE: this should handle correctly charge register wraps around.
  const int16 charge_reading_diff = (current_report_charge_reading - last_report_charge_reading);
  last_report_charge_reading = current_report_charge_reading;
    
  // TODO: define as const at the begining of the file.
  // TODO: refactor to seperate conversion and formatting methods.
  const float kLtc2943ChargeLsb = 0.34E-3;
  const float kShuntResistorOhms = 0.025;
  // TODO: assert that this is acutally what we set.
  const uint16 kChargePrescaler = 1;
  const float kAvgMahPerChargeTickRatio = ((1000 * kLtc2943ChargeLsb * kChargePrescaler * 50e-3)/(kShuntResistorOhms * 4096));
  const float kAvgMaPerChargeTickRatio = kAvgMahPerChargeTickRatio * 60 * 60 * 1000 / kReportingPeriodMillis;

  const int32 avg_micro_amps = (int32)(charge_reading_diff * kAvgMaPerChargeTickRatio * 1000);
  const int16 amps = avg_micro_amps / 1000000L;
  const int32 ua_fraction = avg_micro_amps - (amps * 1000000L);
  const int16 ma = ua_fraction / 1000;
  const int16 ua = ua_fraction - (ma * 1000);

  if (config::isDebug()) {
    sio::printf(F("%04x, %d, %d.%03d %03d\n"), last_report_charge_reading, charge_reading_diff, amps, ma, ua);
  } else {
     sio::printf(F("%d.%03d%03d\n"), amps, ma, ua);  
  }
  leds::activity.action(); 
  //sio::printf(F("Config: %02x\n"), config::get());
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


