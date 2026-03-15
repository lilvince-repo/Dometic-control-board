#include "aes_fsm.h"

// Keep the RB interrupt for light control exactly like old code
static short a;

#INT_RB
void RB_isr(void) {
  clear_interrupt(INT_RB);
  a = input_state(Lichtschalter);
  if (a == 0) output_high(Licht);
  else        output_low(Licht);
}

void main() {
  aes_fsm_t fsm;

  aes_hw_adc_init();
  aes_hw_inputs_init();
  aes_hw_setpoint_init();
  aes_hw_light_init();

  // initial measurement + power-on indication similar to old code
  aes_hw_adc_step();

  if (aes_ac_present()) {
    unsigned int8 k;
    for (k = 0; k < 10; k++) {
      output_toggle(LED);
      delay_ms(100);
    }
    output_low(LED);
  } else {
    output_low(LED);
  }

  delay_ms(5000);

  enable_interrupts(INT_RB);
  enable_interrupts(GLOBAL);

  aes_hw_all_outputs_safe_off();
  aes_fsm_init(&fsm);

  while (1) {
    aes_fsm_step(&fsm);
    // Small idle delay to reduce CPU burn; still responsive
    delay_ms(10);
  }
}