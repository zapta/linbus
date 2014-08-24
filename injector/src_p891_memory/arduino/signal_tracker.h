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

#ifndef SIGNAL_TRACKER_H
#define SIGNAL_TRACKER_H

#include "system_clock.h"
#include "passive_timer.h"

// A class for trackign and conditioning boolean signal reports extracted from
// intercepted linbus frames.
class SignalTracker {
public:

  // Tracked signal states. Using uint8 instead of enums which are int16.
  struct States {
    static const uint8 UNKNOWN = 0;
    static const uint8 OFF = 1;
    static const uint8 ON = 2;
   private:
    // do not instantiate.
    States() {
    }  
  };

  // Constctor and configuration.
  // * Definition: Supporting report - a call to reportSignal with a value that matches the current state.
  // * Definition: Pending report - a call to reportSignal with a value that does not match the current state. A sequence of 
  // identical pending reports is required to change a state.
  //
  // * required_pending_reports_count - number of equal consecutive pending reports required to change a state.
  // * pending_report_ttl_millis - max time in millis between pending reports to consider consecutive.
  // * supporting_report_ttl_millis - max time in millis from the last supporting report before changing state to UNKNOWN.
  SignalTracker(uint8 required_pending_reports_count, uint16 pending_report_ttl_millis, uint16 supporting_report_ttl_millis) 
  :  
    required_pending_reports_count_(required_pending_reports_count),
    pending_report_ttl_millis_(pending_report_ttl_millis),
    supporting_report_ttl_millis_(supporting_report_ttl_millis)
    {
      enterStateUnknown();
    }


  // Called from the main loop() method.
  // May change the state to UNKNOWN.
  inline void loop() {
    // Check for state expiration due to lack of supporting reports.
    if (state_ == States::UNKNOWN || 
        time_since_last_supporting_report_.timeMillis() < supporting_report_ttl_millis_) {
      return;
    }
    // This also resets the pending reports.
    enterStateUnknown();
  }

  // Accept a report about the current value of the signal. apply filtering
  // logic and if conditions met, change the state to ON or OFF.
  inline void reportSignal(boolean is_on) {
    // Handle the case where the report matches the current state.
    if (doesReportSupportCurrentState(is_on)) {
      time_since_last_supporting_report_.restart();
      return;
    }

    // Here when the report contradicts the current state. That is, this is a 
    // pending report.

    // Is this the first report in the sequence of pending reports or an addition
    // to an existing sequence with the same signal value.
    const boolean isFirstConsecutiveReport = 
      (consecutive_pending_reports_count_ == 0) || 
      (consecutive_pending_reports_value_ != is_on) || 
      (time_since_last_pending_report_.timeMillis() >= pending_report_ttl_millis_);

    // Handle the case of first consecutive pending report with this value.
    if (isFirstConsecutiveReport) {
      consecutive_pending_reports_count_ = 1;
      consecutive_pending_reports_value_ = is_on;
    } 
    else {
      // Handle the case of non first consecutive pending report. 
      // NOTE: should not overflow since we limit this to required_report_count.
      consecutive_pending_reports_count_++;
    }

    time_since_last_pending_report_.restart();   

    // If insufficient number of consecutive pending reports to change state, do nothing.
    if (consecutive_pending_reports_count_ < required_pending_reports_count_) {
      return;
    }

    // Here when enough consecutive pending reports to change state.

    // The pending report supports the new states.
    time_since_last_supporting_report_.restart();
    time_in_state_.restart();
    consecutive_pending_reports_count_ = 0;
    state_ = (is_on) ? States::ON : States::OFF;
  }

  // Retrieves the current state. Returns one of States values. Does not change state.
  inline uint8 state() const {
    return state_;
  }
  
  // Convinience method.
  inline boolean isOn() const {
    return state_ == States::ON;
  }
  
  // Convinience method.
  inline boolean isOff() const {
    return state_ == States::OFF;
  }
  
  // Convinience method.
  inline boolean isOnForAtLeastMillis(uint32 min_time_in_state) const {
    return isOn() &&  (timeInStateMillis() >= min_time_in_state);
  }
  
  // Convinience method.
  inline boolean isKnown() const {
    return state_ != States::UNKNOWN;
  }

  // Returns time in millis in current state. Since getState() does not change state, calling it
  // after getState() will return the time for the state returns by getState().  (that is, no race
  // condition.). 
  inline uint32 timeInStateMillis() const {
    return time_in_state_.timeMillis();
  }

private:
  const uint8 required_pending_reports_count_;
  const uint16 pending_report_ttl_millis_;
  const uint16 supporting_report_ttl_millis_;

  // The current state. One of States values.
  uint8 state_;

  // Time in the current state state_.
  PassiveTimer time_in_state_;

  // Time of last report that supported the current state. No used when in
  // UNKNOWN state.
  PassiveTimer time_since_last_supporting_report_;


  // Number of consecutive and equal reports that do no match the current state.
  uint8 consecutive_pending_reports_count_;

  // Valid only if consecutive_pending_reports_count_ > 0;  Holds the value of the
  // pending reports.
  boolean consecutive_pending_reports_value_;

  // Valid only if consecutive_pending_reports_count_ > 0; Represents the time of the
  // last pending report.
  PassiveTimer time_since_last_pending_report_;

  // Does the given value passed to reportSignal() matches the current state ('supporting report')
  // or may influence a change to a different state ('pending report').
  inline boolean doesReportSupportCurrentState(boolean report_is_on) {
    switch (state_) {
      case States::UNKNOWN:  
        // No report matches the UNKNOWN state.
        return false;
      case States::ON: 
        return report_is_on;
      case States::OFF: 
        return !report_is_on;
    }
    // TODO: unexpected state, surface this error somehow.
    return false;
  }

  inline void enterStateUnknown() {
    state_ = States::UNKNOWN;
    time_in_state_.restart();
    consecutive_pending_reports_count_ = 0;
  }
};

#endif






