#include "aes_hw.h"

// -----------------------------------------------------------------------------
// Timebase
// CCS: get_ticks() is available because headers.h uses #use timer(TIMER=1,...)
// -----------------------------------------------------------------------------
unsigned int16 aes_ticks16(void) {
  return get_ticks();
}

// Extend 16-bit tick to 32-bit ms counter (handles wrap)
unsigned long aes_now_ms(void) {
  static unsigned int16 last = 0;
  static unsigned long  acc = 0;

  unsigned int16 t = get_ticks();
  unsigned int16 dt = (unsigned int16)(t - last);
  last = t;
  acc += (unsigned long)dt;
  return acc;
}

// -----------------------------------------------------------------------------
// Debounced inputs
// -----------------------------------------------------------------------------
typedef struct {
  short stable;
  short raw_last;
  unsigned long last_change_ms;
} debounced_t;

static debounced_t db_ac;
static debounced_t db_dplus;
static debounced_t db_solar;

static void debounce_init(debounced_t* d, short initial_raw) {
  d->stable = initial_raw;
  d->raw_last = initial_raw;
  d->last_change_ms = aes_now_ms();
}

static void debounce_step(debounced_t* d, short raw) {
  unsigned long now = aes_now_ms();

  if (raw != d->raw_last) {
    d->raw_last = raw;
    d->last_change_ms = now;
  }

  if ((unsigned long)(now - d->last_change_ms) >= AES_DEBOUNCE_MS) {
    d->stable = d->raw_last;
  }
}

void aes_hw_inputs_init(void) {
  // raw readings at init
  debounce_init(&db_ac,    input(Detect230V)); // active-low
  debounce_init(&db_dplus, input(DDetect));
  debounce_init(&db_solar, input(Solar));
}

void aes_hw_inputs_step(void) {
  debounce_step(&db_ac,    input(Detect230V));
  debounce_step(&db_dplus, input(DDetect));
  debounce_step(&db_solar, input(Solar));
}

short aes_ac_present(void) {
  // Detect230V is active-low: LOW -> AC present
  return (db_ac.stable == 0);
}

short aes_engine_running(void) {
  return (db_dplus.stable != 0);
}

short aes_solar_active(void) {
  // solar is treated by FSM as "parked only"
  // Original code: while (!Input(Solar)) means solar active when LOW.
  return (db_solar.stable == 0);
}

// -----------------------------------------------------------------------------
// ADC (voltage + temperature averaging) (ported from your messen())
// -----------------------------------------------------------------------------
static unsigned long last_adc_ms = 0;
static unsigned int16 vbat_adc = 0;  // mittels
static unsigned int16 temp_adc = 0;  // mittel

void aes_hw_adc_init(void) {
  setup_adc_ports(AN0_AN1_AN3);
  setup_adc(ADC_CLOCK_INTERNAL);
  last_adc_ms = 0;
}

static void adc_measure_now(void) {
  unsigned int16 j;
  unsigned int16 sum;

  // Channel 0: voltage sense
  set_adc_channel(0);
  delay_us(100);
  sum = 0;
  for (j = 0; j < 32; j++) {
    sum += read_adc();
    delay_us(100);
  }
  vbat_adc = sum / 32;

  // Channel 3: temperature
  set_adc_channel(3);
  delay_us(100);
  sum = 0;
  for (j = 0; j < 32; j++) {
    sum += read_adc();
    delay_us(100);
  }
  temp_adc = sum / 32;

  // Keep the legacy voltage pulse signaling
  if (vbat_adc > AES_VOLTAGE_PULSE_THRESHOLD) {
    aes_hw_signal_pulse(56);
  } else {
    aes_hw_signal_pulse(57);
  }
}

void aes_hw_adc_step(void) {
  unsigned long now = aes_now_ms();
  if (last_adc_ms == 0 || (unsigned long)(now - last_adc_ms) >= AES_ADC_PERIOD_MS) {
    adc_measure_now();
    last_adc_ms = now;
  }
}

unsigned int16 aes_hw_get_temp_adc(void) {
  return temp_adc;
}

unsigned int16 aes_hw_get_vbat_adc(void) {
  return vbat_adc;
}

// -----------------------------------------------------------------------------
// UART setpoint selection (ported from your sollwert())
// -----------------------------------------------------------------------------
static unsigned int16 setpoint_adc = 433; // default ~4°C equivalent (matches '3')

void aes_hw_setpoint_init(void) {
  setpoint_adc = 433;
}

static void apply_setpoint_char(int ch) {
  if (ch == 49) setpoint_adc = 385; // '1'
  if (ch == 50) setpoint_adc = 409; // '2'
  if (ch == 51) setpoint_adc = 433; // '3'
  if (ch == 52) setpoint_adc = 456; // '4'
  if (ch == 53) setpoint_adc = 480; // '5'
}

void aes_hw_setpoint_step(void) {
  if (kbhit()) {
    int ch = getch();
    apply_setpoint_char(ch);
  }
}

unsigned int16 aes_hw_get_setpoint_adc(void) {
  return setpoint_adc;
}

// -----------------------------------------------------------------------------
// Outputs
// -----------------------------------------------------------------------------
void aes_hw_signal_pulse(unsigned char n) {
  unsigned int16 j;
  unsigned char Buffer = n;

  output_low(Signal);
  delay_us(100);
  for (j = 0; j < 8; j++) {
    output_bit(Signal, shift_right(&Buffer, 1, 0));
    delay_us(100);
  }
  output_high(Signal);
  delay_us(1);
}

void aes_hw_beep(unsigned int8 n) {
  unsigned int8 k;
  for (k = 1; k <= n; k++) {
    output_high(Summer);
    delay_ms(100);
    output_low(Summer);
    delay_ms(100);
  }
}

void aes_hw_set_led(short on) {
  if (on) output_high(LED);
  else    output_low(LED);
}

void aes_hw_set_ac_heater(short on) {
  if (on) output_high(Relais230V);
  else    output_low(Relais230V);
}

void aes_hw_set_12v_heater(short on) {
  if (on) output_high(Relais12V);
  else    output_low(Relais12V);
}

void aes_hw_gas_outputs_off(void) {
  output_low(Zunder);
  output_low(Gassi);
  output_low(Gasventil);
}

void aes_hw_gas_outputs_ignite_on(void) {
  // Preserve original ignition ON polarity
  output_high(Zunder);
  output_high(Gassi);
  output_high(Gasventil);
}

void aes_hw_all_outputs_safe_off(void) {
  aes_hw_set_ac_heater(0);
  aes_hw_set_12v_heater(0);
  aes_hw_gas_outputs_off();
  aes_hw_set_led(0);
  output_low(Summer);
}

void aes_hw_light_init(void) {
  output_low(Licht);
}