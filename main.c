#include <aes_V4_ohne_Piep.h>

int ch,i,p,Modus;
short a,flag;
short unter=0;
unsigned int16 j, soll, mittel, mittels, temp, Spannung;
unsigned int16 current_tick, previous_tick;

//=============================================================================
unsigned int16 tick_difference(unsigned int16 current, unsigned int16 
previous) {
 return(current - previous);
}
//============================================================================= Temperatur-Sollwerte

void sollwert ()
{
   if (ch==49) soll=385;      //entspricht 8�C
   if (ch==50) soll=409;      //entspricht 6�C
   if (ch==51) soll=433;      //entspricht 4�C
   if (ch==52) soll=456;      //entspricht 2�C
   if (ch==53) soll=480;      //entspricht 0�C
}

//=============================================================================
void Pulse(n)
{
   unsigned char Buffer=n;
   output_low(Signal);       //Startimpuls
   delay_us(100);
   for(j=0;j<8;j++)
   {
      output_bit(Signal,shift_right(&Buffer,1,0));
      delay_us(100);
   }
   output_high(Signal);
   delay_us(1);
}
//============================================================================= Messung Temperatur & Spannung
void messen()
{  
    set_adc_channel(0);
    delay_us(100);
    Spannung=0;
    mittels=0;
    for (j=0;j<32;j++)
      {  Spannung=read_adc();
         delay_us(100);
         mittels=mittels + Spannung;
      }
   mittels=mittels/32;
   //printf("Spannung: %Lu\r\n",mittels);
   set_adc_channel(3);
   delay_us(100);  
   mittel=0;
   temp=0;
   for (j=0;j<32;j++)
   {  temp=read_adc();
      delay_us(100);
      mittel=mittel + temp;
   }
   mittel=mittel/32;
   //printf("Temperatur: %Lu\r\n",mittel);
   //printf("Sollwert: %Lu\r\n",soll);
   if (mittels > 656)pulse(56);
   else if (mittels <= 656) pulse(57);  
}

//============================================================================= Piepen
void beep(i)
{  for (j=1;j<=i;j++)
   {  output_high(Summer);
      delay_ms(100);
      output_low(Summer);
      delay_ms(100);
   }
}

//============================================================================= Gaszyklus
void gascycle(void)
{
   int16 bcl2;
   unsigned char Flamme;
   setup_adc_ports(NO_ANALOGS);
   delay_ms(1000);                     // 1 sec Pause
   output_high(Zunder);                //Z�nder an
   output_high(Gassi);                 //Z�ndsicherung an
   output_high(Gasventil);             //Gasrelais an
   bcl2=300;                           //300*10ms=3sec
   Flamme=0;                           //Flamme an-Flag, wenn auf 0 bleibt --> Alarm
   p=0;                                
//============================================================================= Gaszyklus
// Gasventil ist auf, Z�nder an
// Wenn kein Funken vorhanden --> ALARM
   do                                  //
   {
      if ( !input(Zundubw) )           //wenn Funken vorhanen (LOW)
      {
         bcl2=1;                       //Schleife vor 3 Sekunden verlassen
         Flamme=1;                     //kein Alarm
      }
      delay_ms(10);
   }
   while (bcl2-- >0);                   //Schleife 3 sec

   if (Flamme==0)                      // Wenn 3 sec kein Funke vorhanden --> Alarm
   {  
      output_low(Zunder);              //Z�nder aus
      output_low(Gasventil);           //Gasrelais aus
      output_low(Gassi);               //Z�ndsicherung aus
      pulse(55);
      beep(3);                         //Alarm
      While(1)                         //Verriegeln
      { pulse(55);
        delay_ms(500);   }
   }
//============================================================================= Gaszyklus
// Gasventil ist auf, Z�nder an
// Funken vorhanden, 15 Sekunden abwarten, keine Flamme --> ALARM
   erneut:
   p=0;
   current_tick = previous_tick = get_ticks();
   while(tick_difference(current_tick, previous_tick) < 15000)     //15 Sekunden Schleife
   {  
      if ( !input(Zundubw) )          //wenn Funken vorhanen (LOW)
      {  
         p=p+1;
         delay_ms(100);                                                          //delay so lange wie ein Piep
         //beep(1);
      }
      current_tick = get_ticks();
   }  
   
   if (p>=28)                          //Wenn mehr als 30 mal gez�ndet
   {
      output_low(Zunder);              //Z�nder aus
      output_low(Gasventil);           //Gasrelais aus
      output_low(Gassi);               //Z�ndsicherung aus
      delay_ms(100);
      pulse(55);
      beep(5);                         //Alarm
      pulse(55);
      While(1)                         //Verriegeln
      { pulse(55);
        delay_ms(500);   }
   }
   else if (p<30) Flamme=1;
   
   //setup_adc_ports(AN0_AN1_AN3);                    // Configure AN1 as analog
   //set_adc_channel(3);                              // Select channel 3 (AN3) input
   //setup_adc(ADC_CLOCK_INTERNAL);
   do
   {
      output_low(Gasventil);           //Gasrelais aus
      current_tick = get_ticks();
      if (tick_difference(current_tick, previous_tick) > 1000)     //1 Sekunden Schleife
      {  
         messen();
         previous_tick = current_tick;
      }
      if (kbhit() )
         {
         ch = getch();              
         Sollwert();
         }
      if (!input(Zundubw))
      {
         Flamme=0; 
         goto erneut;
      }
      if (Input(DDetect)) Flamme=0;
      if (!Input(Detect230V)) Flamme=0;
      if (mittel > soll) Flamme=0;                 //5�C
   }
   while(Flamme == 1);   // muss den niedrigen Sollwert erreichen
}
//=============================================================================

#INT_RB
void  RB_isr(void) 
{
   clear_interrupt(INT_RB);   
   a = input_state(Lichtschalter);     //RB4 = Lichtsensor
   if (a==0) {output_high(Licht); }
   else {output_low(Licht); }
   //b=input_state(DDetect);             //RB6 = D+-Detect
}
//============================================================================= Hauptprogramm
void main()
{
   setup_adc_ports(AN0_AN1_AN3);                    // Configure AN1 as analog
   setup_adc(ADC_CLOCK_INTERNAL);
   messen();
   if (!input_state(Detect230V))
   {
      for (j=1;j<=10;j++)                   //Wenn 230V annliegen, 5 mal schnelles Blinken
      {
         output_toggle(LED);
         delay_ms(100);
      }
      output_low(LED);
   }
   else output_low(LED);
   Modus=MODUSGAS;
   delay_ms(5000); 
   enable_interrupts(int_RB);
   enable_interrupts(GLOBAL);
   output_low(Licht);
              
   while (1)
   {
   current_tick = previous_tick = get_ticks();

//============================================================================= Solarmodus
      while (!Input(Solar)&&(unter==0))
      {
         output_low(Relais230V);                   //
         output_low(Gassi);                        //Sicherheit
         output_low(Gasventil);                    //
         output_low(Zunder);                       //
         //Modus=MODUS12V;
         current_tick = get_ticks();
         if (tick_difference(current_tick, previous_tick) > 1000)     //1 Sekunden Schleife
            {  
            messen();
            previous_tick = current_tick;
            }
         if (mittel > soll)         //5�C
            {   
            output_low(Relais12V);
            output_low(LED);
            }
         else if (mittel < soll)
            {
            output_high(Relais12V);
            output_high(LED);
            }
         if (kbhit() )
            {
            ch = getch();              // 290 entspricht 17,5�C
            Sollwert();        
            }
         if (mittels <= 827)              //Spannnung kleiner 12V? 
            {                            
            unter=1;                      //Unterspannungsflag an
            }
         else if (mittels > 827)          //Spannnung gr��er 12V? 
            {                           
            unter=0;                      //Unterspannungsflag aus
            }
      }
      output_low(LED);

//============================================================================= 230V-Modus
   while(!Input(Detect230V))
      {
         output_low(Gassi);                        //Sicherheit
         output_low(Gasventil);                    //
         output_low(Zunder);                       //
         output_low(Relais12V);                    //
         Modus=MODUS230V;
         current_tick = get_ticks();
         if (tick_difference(current_tick, previous_tick) > 1000)     //1 Sekunden Schleife
            {  
               messen();
               previous_tick = current_tick;
            }
         if (mittel > soll)         
            {   
               output_low(LED);
               output_low(Relais230V);
               
            }
         else if (mittel < soll)
            {
            output_high(Relais230V);
            output_high(LED);
            
            }
         if (kbhit() )
            {                             //421 entspricht 5�C
            ch = getch();              // 290 entspricht 17,5�C
            Sollwert();
            }
      }
      output_low(LED);
          
//============================================================================= 12V-Modus  
      while(Input(DDetect) && Input(Detect230V))
      {
         if (Modus != MODUS12V)  delay_ms(1000);   // wenn wir aus einem anderen Modus kommen, 1 sec Pause
         output_low(Relais230V);                   //
         output_low(Gassi);                        //Sicherheit
         output_low(Gasventil);                    //
         output_low(Zunder);                       //
         Modus=MODUS12V;
         current_tick = get_ticks();
         if (tick_difference(current_tick, previous_tick) > 1000)     //1 Sekunden Schleife
            {  
            messen();
            previous_tick = current_tick;
            }
         if (mittel > soll)         //5�C
            {   
            output_low(Relais12V);
            output_low(LED);
            }
         else if (mittel < soll)
            {
            output_high(Relais12V);
            output_high(LED);
            }
         if (kbhit() )
            {
            ch = getch();
            Sollwert();
            }
      }
//============================================================================= Gasmodus

// Bevor Sie in den GAS-Modus wechseln, m�ssen Sie sicherstellen, dass Sie sich nicht zuvor im 12-V-Modus
// befunden haben, andernfalls 15 Minuten warten! Nicht an Tankstellen einschalten.
      flag = 1;
      if (Modus != MODUSGAS) delay_ms(1000);
      while(flag)
      {  
         output_low(Relais230V); 
         output_low(Relais12V); 
         output_low(LED);
         if (Modus==MODUS12V)
         {                             //Schleife  900 x 1 sec = 15min 
            for (j=0;j<900;j++)
            {
               output_high(LED);
               delay_ms(100);
               output_low(LED);
               delay_ms(900);
               if (Input(DDetect) || !Input(Detect230V)) break;
            }
            Modus=MODUSGAS;
         }
         flag=1;
         if (Input(DDetect)) flag=0;
         if (!Input(Detect230V)) flag=0;
         
         while(flag)
         {                            // also der Gaskreislauf ist in Ordnung 
            Modus=MODUSGAS;
            //if (MTON) {flag = 0;};      // si retour 12V
            //if (! FORCE220V) {flag = 0;};   // si retour 22OV
            current_tick = get_ticks();
            if (tick_difference(current_tick, previous_tick) > 1000)     //1 Sekunden Schleife
               {  
               messen();
               previous_tick = current_tick;
               }
            if (mittel < soll)               //5�C
               {                
               gascycle();              //K�hlen
               }
            else if (mittel > soll)
               {  // printf("\r\nGAZ_OFF");
               output_low(Gassi);                        //Sicherheit
               output_low(Gasventil);                    //
               output_low(Zunder);;
               }
            if (kbhit() )
               {
               ch = getch();
               Sollwert();
               }
            if (Input(DDetect)) flag=0;
            if (!Input(Detect230V)) flag=0; 
         }
      }
      output_Low(LED);
      //output_low(Relais12V);
      output_low(Summer);
   }
}
//=============================================================================
