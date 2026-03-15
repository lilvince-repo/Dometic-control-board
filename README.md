# Dometic-control-board (PIC16F819, CCS C)

This firmware controls a 3-way absorption refrigerator with AES-like logic:
- 230Vac heater relay
- 12Vdc heater relay (D+ / engine running)
- Gas ignition + safety outputs

## Energy priority (continuous)
Evaluated continuously (not only at startup):
1. **230Vac** (Detect230V is active-low)
2. **12V (D+)**
3. **Solar (parked only)**: Solar active AND D+ not present AND battery not undervoltage
4. **Gas**

## Setpoint selection
UART receives ASCII '1'..'5' to select one of 5 setpoints (ADC values preserved from original firmware).

## ADC sampling
Every 1 second:
- AN0 averaged (32 samples): `vbat_adc`
- AN3 averaged (32 samples): `temp_adc`

Voltage pulse signaling preserved:
- Pulse(56) if vbat_adc > 656
- Pulse(57) otherwise

## Gas state machine (non-blocking)
Gas operation is a sub-state machine so that AC/D+ appearing will interrupt gas immediately.

States:
- `GAS_PRESTART` (1s)
- `GAS_SPARK_CHECK` (3s): must see spark feedback (`!input(Zundubw)`)
- `GAS_IGNITION_WINDOW` (15s): count spark events; fault if >= 28
- `GAS_RUN`: run until setpoint reached or higher-priority energy appears
- `GAS_FAULT_WAIT`: 10 minutes, then retry if still needed

### Fault behavior (no lockout)
Old firmware used infinite loops on gas faults. This refactor removes all hard lockouts:
- On gas failure: Pulse(55), beep pattern, enter 10-minute wait
- While waiting: keep checking AC/D+. If AC/D+ returns, switch immediately.

### Beeping
- On fault detection: `beep(3)` for spark-check fail, `beep(5)` for ignition-window fail.
- During 10-minute wait: `beep(1)` once per minute.

## 15-minute inhibit (after leaving 12V)
If the controller transitions from **12V(D+) to Gas**, it enforces a **15 minute inhibit** before attempting gas ignition (non-blocking). AC/D+ can still preempt during this time.