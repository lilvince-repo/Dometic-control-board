#ifndef AES_HW_H
#define AES_HW_H

#include "aes_config.h"

// Simple tick utilities
unsigned int16 aes_ticks16(void);
unsigned long  aes_now_ms(void);

// Debounced supply inputs
void  aes_hw_inputs_init(void);
void  aes_hw_inputs_step(void);

short aes_ac_present(void);        // Detect230V active-low
short aes_engine_running(void);    // DDetect
short aes_solar_active(void);      // Solar input (raw), interpretation done in FSM

// ADC sampling
void aes_hw_adc_init(void);
void aes_hw_adc_step(void);        // periodic measurement scheduler

unsigned int16 aes_hw_get_temp_adc(void);    // mittel
unsigned int16 aes_hw_get_vbat_adc(void);    // mittels

// Setpoint / UART
void aes_hw_setpoint_init(void);   // set default
void aes_hw_setpoint_step(void);   // handle UART
unsigned int16 aes_hw_get_setpoint_adc(void);

// Outputs / helpers
void aes_hw_all_outputs_safe_off(void);

void aes_hw_set_led(short on);

void aes_hw_set_ac_heater(short on);    // Relais230V
void aes_hw_set_12v_heater(short on);   // Relais12V

void aes_hw_gas_outputs_off(void);
void aes_hw_gas_outputs_ignite_on(void); // Zunder + Gassi + Gasventil ON (as original)

void aes_hw_signal_pulse(unsigned char code);
void aes_hw_beep(unsigned int8 n);

// Light handling ISR hook (kept as in old firmware)
void aes_hw_light_init(void);

#endif // AES_HW_H