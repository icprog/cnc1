/******************************************************************************/
/*Filename:    AVR.h                                                          */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Definitions for AVR platform independent functions.            */
/******************************************************************************/
#include <stddef.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "BootExport.h"
#include "AVR.h"

/*Global variables*/
static unsigned char overflow;
static unsigned int matches;


/* Initialize AVR platform.
 *
 * INPUT : [None]
 *
 * OUTPUT: [None]
 */
void Platform_Open(void)
{
   /*Setup pins I/O*/
   DDRA = 0xFF;
   DDRB = 0x76;
   DDRC = 0x88;
   DDRD = 0xFF;

   /* Enable internal pull-up's on unused pins,
    * set LOW on used output pins.
    *
    * LCD & TP share pins, ensure CS is HIGH
    * initially for both (PC3 & PB2).
    */
   PORTB = 0x85;
   PORTC = 0x7C;
   PORTA = 0x00;
   PORTD = 0x00;
}


/* Cleanup AVR specific resources.
 *
 * INPUT : [None]
 *
 * OUTPUT: [None]
 */
void Platform_Close(void)
{

}


/* Enable global interrupts.
 *
 * INPUT : enable - true to enable interrupts, false otherwise
 *
 * OUTPUT: [None]
 */
void Platform_EnableInterrupts(bool enable)
{
   if(enable)
   {
      sei();
   }
   else
   {
      cli();
   }
}


/* Set timer counters and callback.
 *
 * INPUT : ms - time to delay until callback
 *         cb - callback when time elapses
 *
 * OUTPUT: [None]
 */
void Platform_SetTimer(unsigned int ms, void (*cb)(void))
{
   xxPlatform_RegisterCB(cb, 0x02);
   matches = ms;
   overflow = 0x05;

   TCCR0A = 0x00;
   TIMSK0 = 0x01;
   OCR0A = 0xDC;
   TCCR0B = 0x02;
}


/* Set/Get interrupt callback on/from internal callback list.
 *
 * INPUT : cb - callback to set/get on/from internal list
 *         index - index of callback
 *
 * OUTPUT: [Return] - callback at index
 */
void (*xxPlatform_RegisterCB(void (*cb)(void), unsigned char index))(void)
{
   static void (*cbList[0x03])(void);

   /*Add callback only if non-NULL*/
   if(cb != NULL)
   {
      cbList[index] = cb;
   }

   return cbList[index];
}


/*Timer0 Compare Match A interrupt vector*/
ISR(TIMER0_COMPA_vect)
{
   matches--;
   if(matches)
   {
      overflow = 0x05;
      TCNT0 = 0x00;
      TIMSK0 = 0x01;
   }
   else
   {
      TCCR0B = 0x00;
      xxPlatform_RegisterCB(NULL, 0x02)();
   }
}


/*Timer0 Overflow interrupt vector*/
ISR(TIMER0_OVF_vect)
{
   overflow--;
   if(!overflow)
   {
      TIMSK0 = 0x02;
   }
}


/*TWI read-INT interrupt vector*/
ISR(PCINT2_vect)
{
   if(PCMSK2 & 0x01)
   {
      xxPlatform_RegisterCB(NULL, 0x00)();
   }
}


/*Touch panel interrupt vector*/
ISR(PCINT1_vect)
{
   if(PCMSK1 & 0x08)
   {
      xxPlatform_RegisterCB(NULL, 0x01)();
   }
}


/*TWI interrupt vector*/
ISR(TWI_vect)
{
   TWI_ISR();
}
