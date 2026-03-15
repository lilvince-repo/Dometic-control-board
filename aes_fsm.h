#ifndef AES_FSM_H
#define AES_FSM_H

#include "aes_hw.h"

typedef struct {
  aes_mode_t   mode;
  aes_mode_t   prev_mode;

  gas_state_t  gas_state;

  unsigned long gas_state_started_ms;
  unsigned long gas_fault_until_ms;
  unsigned long gas_inhibit_until_ms;

  // ignition pulse counter in ignition window
  unsigned int16 gas_pulse_count;

  // alarm timing during fault wait
  unsigned long last_fault_beep_ms;
  unsigned long last_fault_pulse_ms;

  // used for "spark seen" during spark check
  short spark_seen;

} aes_fsm_t;

void aes_fsm_init(aes_fsm_t* f);
void aes_fsm_step(aes_fsm_t* f);

#endif // AES_FSM_H