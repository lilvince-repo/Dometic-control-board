#include "aes_fsm.h"

// Helpers
static short need_cooling(void) {
  // Preserve original semantics: if temp ADC is below setpoint, cool.
  // (Your code uses: if (mittel < soll) gascycle();)
  return (aes_hw_get_temp_adc() < aes_hw_get_setpoint_adc());
}

static short stop_cooling(void) {
  return (aes_hw_get_temp_adc() > aes_hw_get_setpoint_adc());
}

static aes_mode_t compute_desired_mode(void) {
  // Priority: AC > 12V(D+) > Solar(parked) > Gas
  if (aes_ac_present()) return AES_MODE_AC;
  if (aes_engine_running()) return AES_MODE_12V;

  // Solar only when parked (no D+). Solar input is active when LOW.
  // Also require voltage not undervoltage (same threshold as original solar logic).
  if (aes_solar_active() && (aes_hw_get_vbat_adc() > AES_SOLAR_UNDERVOLT_THRESHOLD)) {
    return AES_MODE_SOLAR;
  }

  return AES_MODE_GAS;
}

static void enter_mode(aes_fsm_t* f, aes_mode_t new_mode) {
  f->prev_mode = f->mode;
  f->mode = new_mode;

  // Any transition: make outputs safe first
  aes_hw_set_ac_heater(0);
  aes_hw_set_12v_heater(0);
  aes_hw_gas_outputs_off();
  aes_hw_set_led(0);

  // If transitioning from 12V to gas, apply 15min inhibit
  if (f->prev_mode == AES_MODE_12V && new_mode == AES_MODE_GAS) {
    f->gas_inhibit_until_ms = aes_now_ms() + AES_GAS_INHIBIT_AFTER_12V_MS;
  }

  // When leaving GAS mode, reset gas state
  if (new_mode != AES_MODE_GAS) {
    f->gas_state = GAS_IDLE;
    f->gas_state_started_ms = aes_now_ms();
    f->spark_seen = 0;
    f->gas_pulse_count = 0;
  }
}

static void gas_set_state(aes_fsm_t* f, gas_state_t st) {
  f->gas_state = st;
  f->gas_state_started_ms = aes_now_ms();
}

static void gas_alarm_tick(aes_fsm_t* f) {
  unsigned long now = aes_now_ms();

  // repeat Pulse(55) every ~500ms while in fault wait
  if (f->last_fault_pulse_ms == 0 || (unsigned long)(now - f->last_fault_pulse_ms) >= 500u) {
    aes_hw_signal_pulse(55);
    f->last_fault_pulse_ms = now;
  }

  // beep once per minute (as agreed)
  if (f->last_fault_beep_ms == 0 || (unsigned long)(now - f->last_fault_beep_ms) >= AES_FAULT_BEEP_PERIOD_MS) {
    aes_hw_beep(AES_FAULT_BEEP_PULSES);
    f->last_fault_beep_ms = now;
  }
}

static void gas_step(aes_fsm_t* f) {
  unsigned long now = aes_now_ms();

  // If higher-priority source appears, abort gas immediately
  if (aes_ac_present() || aes_engine_running()) {
    aes_hw_gas_outputs_off();
    gas_set_state(f, GAS_IDLE);
    return;
  }

  // If not cooling demand, keep gas off
  if (!need_cooling()) {
    aes_hw_gas_outputs_off();
    gas_set_state(f, GAS_IDLE);
    return;
  }

  // Inhibit after leaving 12V mode
  if ((long)(now - f->gas_inhibit_until_ms) < 0) {
    // inhibited; blink LED slowly to indicate waiting
    // (non-blocking: just toggle LED based on time)
    if ((now / 1000u) & 1u) aes_hw_set_led(1);
    else                    aes_hw_set_led(0);

    aes_hw_gas_outputs_off();
    return;
  }

  // Fault wait
  if (f->gas_state == GAS_FAULT_WAIT) {
    aes_hw_gas_outputs_off();
    gas_alarm_tick(f);

    if ((long)(now - f->gas_fault_until_ms) >= 0) {
      // retry ignition if still only option + still need cooling
      f->last_fault_beep_ms = 0;
      f->last_fault_pulse_ms = 0;
      gas_set_state(f, GAS_PRESTART);
    }
    return;
  }

  switch (f->gas_state) {
    case GAS_IDLE:
      // start non-blocking ignition sequence
      gas_set_state(f, GAS_PRESTART);
      break;

    case GAS_PRESTART:
      aes_hw_set_led(0);
      aes_hw_gas_outputs_off();
      if ((unsigned long)(now - f->gas_state_started_ms) >= AES_GAS_PRESTART_DELAY_MS) {
        f->spark_seen = 0;
        aes_hw_gas_outputs_ignite_on();
        gas_set_state(f, GAS_SPARK_CHECK);
      }
      break;

    case GAS_SPARK_CHECK:
      aes_hw_set_led(1);
      aes_hw_gas_outputs_ignite_on();

      // detect spark feedback: original code treats !input(Zundubw) as “spark present”
      if (!input(Zundubw)) {
        f->spark_seen = 1;
      }

      if ((unsigned long)(now - f->gas_state_started_ms) >= AES_GAS_SPARK_CHECK_MS) {
        if (!f->spark_seen) {
          // fail: no spark seen
          aes_hw_gas_outputs_off();
          aes_hw_signal_pulse(55);
          aes_hw_beep(3);

          f->gas_fault_until_ms = now + AES_GAS_FAULT_RETRY_MS;
          f->last_fault_beep_ms = 0;
          f->last_fault_pulse_ms = 0;
          gas_set_state(f, GAS_FAULT_WAIT);
        } else {
          f->gas_pulse_count = 0;
          gas_set_state(f, GAS_IGNITION_WINDOW);
        }
      }
      break;

    case GAS_IGNITION_WINDOW:
      aes_hw_set_led(1);
      aes_hw_gas_outputs_ignite_on();

      if (!input(Zundubw)) {
        // Count pulses; add a small delay like original (100ms)
        f->gas_pulse_count++;
        delay_ms(100);
      }

      if ((unsigned long)(now - f->gas_state_started_ms) >= AES_GAS_IGNITION_WINDOW_MS) {
        if (f->gas_pulse_count >= 28) {
          // ignition failed (too many retries)
          aes_hw_gas_outputs_off();
          aes_hw_signal_pulse(55);
          aes_hw_beep(5);
          aes_hw_signal_pulse(55);

          f->gas_fault_until_ms = now + AES_GAS_FAULT_RETRY_MS;
          f->last_fault_beep_ms = 0;
          f->last_fault_pulse_ms = 0;
          gas_set_state(f, GAS_FAULT_WAIT);
        } else {
          gas_set_state(f, GAS_RUN);
        }
      }
      break;

   case GAS_RUN:
    aes_hw_set_led(1);
  
    // Flame assumed established:
    // - pull-in coil OFF
    // - igniter OFF (sparking only during re-ignition)
    // - ignition safety output OFF
    output_low(Gasventil);
    output_low(Zunder);
    output_low(Gassi);
  
    // If igniter activity is detected again, restart ignition window
    if (!input(Zundubw)) {
      f->gas_pulse_count = 0;
      gas_set_state(f, GAS_IGNITION_WINDOW);
      break;
    }
  
    if (stop_cooling()) {
      aes_hw_gas_outputs_off();
      gas_set_state(f, GAS_IDLE);
      break;
    }
    break;

    
    default:
      aes_hw_gas_outputs_off();
      gas_set_state(f, GAS_IDLE);
      break;
  }
}

void aes_fsm_init(aes_fsm_t* f) {
  f->mode = AES_MODE_GAS;
  f->prev_mode = AES_MODE_GAS;

  f->gas_state = GAS_IDLE;
  f->gas_state_started_ms = aes_now_ms();
  f->gas_fault_until_ms = 0;
  f->gas_inhibit_until_ms = 0;
  f->gas_pulse_count = 0;
  f->spark_seen = 0;

  f->last_fault_beep_ms = 0;
  f->last_fault_pulse_ms = 0;
}

void aes_fsm_step(aes_fsm_t* f) {
  // periodic housekeeping
  aes_hw_inputs_step();
  aes_hw_adc_step();
  aes_hw_setpoint_step();

  // Choose mode continuously
  {
    aes_mode_t desired = compute_desired_mode();
    if (desired != f->mode) {
      enter_mode(f, desired);
    }
  }

  // Apply mode outputs
  switch (f->mode) {
    case AES_MODE_AC:
      // ensure gas is off
      aes_hw_gas_outputs_off();
      aes_hw_set_12v_heater(0);

      if (need_cooling()) {
        aes_hw_set_ac_heater(1);
        aes_hw_set_led(1);
      } else if (stop_cooling()) {
        aes_hw_set_ac_heater(0);
        aes_hw_set_led(0);
      }
      break;

    case AES_MODE_12V:
      aes_hw_gas_outputs_off();
      aes_hw_set_ac_heater(0);

      if (need_cooling()) {
        aes_hw_set_12v_heater(1);
        aes_hw_set_led(1);
      } else if (stop_cooling()) {
        aes_hw_set_12v_heater(0);
        aes_hw_set_led(0);
      }
      break;

    case AES_MODE_SOLAR:
      // parked-only solar: uses 12V heater but inhibited on undervoltage
      aes_hw_gas_outputs_off();
      aes_hw_set_ac_heater(0);

      if (aes_hw_get_vbat_adc() <= AES_SOLAR_UNDERVOLT_THRESHOLD) {
        // undervoltage -> don't heat
        aes_hw_set_12v_heater(0);
        aes_hw_set_led(0);
      } else {
        if (need_cooling()) {
          aes_hw_set_12v_heater(1);
          aes_hw_set_led(1);
        } else if (stop_cooling()) {
          aes_hw_set_12v_heater(0);
          aes_hw_set_led(0);
        }
      }
      break;

    case AES_MODE_GAS:
    default:
      aes_hw_set_ac_heater(0);
      aes_hw_set_12v_heater(0);
      gas_step(f);
      break;
  }
}
