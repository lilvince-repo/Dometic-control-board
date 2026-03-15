#ifndef AES_CONFIG_H
#define AES_CONFIG_H

#include "headers.h"

// -----------------------------------------------------------------------------
// Timing constants (ms)
// -----------------------------------------------------------------------------
#define AES_ADC_PERIOD_MS                 1000u

#define AES_DEBOUNCE_MS                   200u

#define AES_GAS_PRESTART_DELAY_MS         1000u
#define AES_GAS_SPARK_CHECK_MS            3000u
#define AES_GAS_IGNITION_WINDOW_MS        15000u

#define AES_GAS_FAULT_RETRY_MS            (10ul * 60ul * 1000ul)   // 10 minutes
#define AES_GAS_INHIBIT_AFTER_12V_MS      (15ul * 60ul * 1000ul)   // 15 minutes

// Alarm beeping during fault-wait
#define AES_FAULT_BEEP_PERIOD_MS          60000u  // 1 minute
#define AES_FAULT_BEEP_PULSES             1u

// -----------------------------------------------------------------------------
// Voltage thresholds (kept from original code)
// -----------------------------------------------------------------------------
#define AES_VOLTAGE_PULSE_THRESHOLD       656u    // Pulse(56) if above else Pulse(57)
#define AES_SOLAR_UNDERVOLT_THRESHOLD     827u    // original solar undervolt inhibit

// -----------------------------------------------------------------------------
// Gas valve behavior during GAS_RUN
//
// Original code does: output_low(Gasventil) inside run loop (line ~154).
// Since your existing firmware "works", keep this behavior configurable.
// If your hardware wants valve ON during run, set this to 1 and rebuild.
// -----------------------------------------------------------------------------
#define AES_GASVALVE_ON_DURING_RUN        0

// -----------------------------------------------------------------------------
// Enums
// -----------------------------------------------------------------------------
typedef enum {
  AES_MODE_AC = 0,     // 230Vac
  AES_MODE_12V = 1,    // D+ running
  AES_MODE_SOLAR = 2,  // Solar when parked
  AES_MODE_GAS = 3
} aes_mode_t;

typedef enum {
  GAS_IDLE = 0,
  GAS_PRESTART,
  GAS_SPARK_CHECK,
  GAS_IGNITION_WINDOW,
  GAS_RUN,
  GAS_FAULT_WAIT
} gas_state_t;

#endif // AES_CONFIG_H