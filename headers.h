#include <16F819.h>
#device ADC=10
#use delay(internal=8000000)
#use timer(TIMER=1,TICK=1ms,BITS=16,NOISR)
#use rs232 (baud=9600, rcv=PIN_A2, xmit=PIN_A4)

#FUSES NOMCLR

#define MODUSGAS     0x00
#define MODUS12V     0x01
#define MODUS230V    0x02

#define Detect12V    PIN_A0
#define Zunder       PIN_A1
#define Signal       PIN_A2
#define Temperatur   PIN_A3
#define LED          PIN_A4
#define Solar        PIN_A5
#define Gassi        PIN_A6
#define Zundubw      PIN_A7
#define Detect230V   PIN_B0
#define Relais12V    PIN_B1
#define Summer       PIN_B2   //  TX
#define Licht        PIN_B3
#define Lichtschalter PIN_B4
#define Relais230V   PIN_B5
#define DDetect      PIN_B6
#define Gasventil    PIN_B7
