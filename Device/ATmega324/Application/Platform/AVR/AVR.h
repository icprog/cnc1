/******************************************************************************/
/*Filename:    AVR.h                                                          */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Declaration/definitions for AVR platform independent functions.*/
/******************************************************************************/
#ifndef AVR_H
#define AVR_H
#include <stdbool.h>
#include <avr/io.h>
#include <avr/cpufunc.h>
#include <util/delay.h>

/* SSD1289 (LCD) - ATmega324:
 * (21-28)D0-D7  - (14-21)PORTD
 * (7-14)D8-D15  - (33-40)PORTA
 * (17)RS        - (5)PB4
 * (16)WR        - (6)PB5
 * (15)RD        - (7)PB6
 * (6)CS         - (25)PC3
 */
#define SSD1289_SET_CS(x)         xxPlatform_SetCS((x))
#define SSD1289_SET_RS(x)         xxPlatform_SetRS((x))
#define SSD1289_SET_RD(x)         xxPlatform_SetRD((x))
#define SSD1289_SET_WR(x)         xxPlatform_SetWR((x))
#define SSD1289_SET_DATA_INPUT(x) xxPlatform_SetInput((x))
#define SSD1289_SET_DATA16(x)     xxPlatform_SetData((x))
#define SSD1289_GET_DATA16()      xxPlatform_GetData()

/* XPT2046 (TP) - ATmega324:
 * (26)DCLK     - (38)PA2
 * (33)OUT      - (34)PA6
 * (31)IN       - (36)PA4
 * (30)CS       - (3)PB2
 * (32)BUSY     - (35)PA5
 * (34)IRQ      - (4)PB3
 */
#define XPT2046_SET_CS(x)        xxPlatform_SetCS2((x))
#define XPT2046_ENABLE_IRQ(x, y) xxPlatform_EnableIRQ((x), (y))
#define XPT2046_SET_INPUT(x)     xxPlatform_SetInput2((x))
#define XPT2046_SET_DCLK(x)      xxPlatform_SetDCLK((x))
#define XPT2046_SET_IN(x)        xxPlatform_SetIN((x))
#define XPT2046_GET_OUT()        xxPlatform_GetOUT()
#define XPT2046_GET_BUSY()       xxPlatform_GetBUSY()

/* TWI (I2C) - ATmega324
 * SCL - (22)SCL
 * SDA - (23)SDA
 */
#define TWI_ENABLE_INT2(x, y) xxPlatform_EnableIRQ2((x), (y))

/* LED    - ATmega324:
 * (0)LED - (29)PC7
 */
#define LED_SET(x) xxPlatform_SetLED((x))

void Platform_Open(void);
void Platform_Close(void);
void Platform_EnableInterrupts(bool);
void Platform_SetTimer(unsigned int, void (*)(void));
void (*xxPlatform_RegisterCB(void (*)(void), unsigned char))(void);


/* Delay execution by variable time in seconds.
 *
 * INPUT : s - seconds to delay execution
 *
 * OUTPUT: [None]
 */
static inline void Platform_Sleep(unsigned int s)
{
   while(s--)
   {
      _delay_ms(0x03E8);
   }
}


/* Delay execution by variable time in milliseconds.
 *
 * INPUT : ms - milliseconds to delay execution
 *
 * OUTPUT: [None]
 */
static inline void Platform_SleepMS(unsigned int ms)
{
   while(ms--)
   {
      _delay_ms(0x01);
   }
}


/* Delay execution by variable time in microseconds.
 *
 * INPUT : us - microseconds to delay execution
 *
 * OUTPUT: [None]
 */
static inline void Platform_SleepUS(unsigned int us)
{
   while(us--)
   {
      _delay_us(0x01);
   }
}


/* Delay execution by variable time in nanoseconds.
 * HACK: Using builtin (which forces define) to get inline behaviour.
 *
 * INPUT : ns - nanoseconds to delay execution
 *
 * OUTPUT: [None]
 */
#define Platform_SleepNS(ns) __builtin_avr_delay_cycles((double)(F_CPU) * \
                                               ((double)ns) / 0x3B9ACA00 + 0.5F)


/* Delay if nanosecond amount of time has not passed.
 *
 * INPUT : last - address of variable to hold last time
 *         ns - nanoseconds that must have passed
 *
 * OUTPUT: last - current time
 */
static inline void Platform_RepeatNS(unsigned int *last, unsigned int ns)
{
   /*AVR doesn't mark time implicitly, therefore sleep full amount*/
   Platform_SleepNS(ns);
}


/*==============================================*/
/* The below set of inline functions complete   */
/* the respective interfaces of the above       */
/* macros.                                      */
/*==============================================*/
static inline void xxPlatform_EnableIRQ2(bool enable, void (*cb)(void))
{
   if(enable)
   {
      xxPlatform_RegisterCB(cb, 0x00);
      PCICR |= 0x04;
      PCMSK2 |= 0x01;
   }
   else
   {
      PCMSK2 &= (~0x01);
   }
}


static inline bool xxPlatform_GetBUSY(void)
{
   return (PINA & 0x20);
}


static inline bool xxPlatform_GetOUT(void)
{
   return (PINA & 0x40);
}


static inline void xxPlatform_SetIN(bool val)
{
   if(val)
   {
      PORTA |= 0x10;
   }
   else
   {
      PORTA &= (~0x10);
   }
}


static inline void xxPlatform_SetDCLK(bool val)
{
   if(val)
   {
      PORTA |= 0x04;
   }
   else
   {
      PORTA &= (~0x04);
   }
}


static inline void xxPlatform_SetInput2(bool input)
{
   if(input)
   {
      DDRA = 0x9F;
   }
   else
   {
      DDRA = 0xFF;
   }
}


static inline void xxPlatform_EnableIRQ(bool enable, void (*cb)(void))
{
   if(enable)
   {
      xxPlatform_RegisterCB(cb, 0x01);
      PCICR |= 0x02;
      PCMSK1 |= 0x08;
   }
   else
   {
      PCMSK1 &= (~0x08);
   }
}


static inline void xxPlatform_SetCS2(bool val)
{
   if(val)
   {
      PORTB |= 0x04;
   }
   else
   {
      PORTB &= (~0x04);
   }
}


static inline void xxPlatform_SetCS(bool val)
{
   if(val)
   {
      PORTC |= 0x08;
   }
   else
   {
      PORTC &= (~0x08);
   }
}


static inline void xxPlatform_SetRS(bool val)
{
   if(val)
   {
      PORTB |= 0x10;
   }
   else
   {
      PORTB &= (~0x10);
   }
}


static inline void xxPlatform_SetRD(bool val)
{
   if(val)
   {
      PORTB |= 0x40;
   }
   else
   {
      PORTB &= (~0x40);
   }
}


static inline void xxPlatform_SetWR(bool val)
{
   if(val)
   {
      PORTB |= 0x20;
   }
   else
   {
      PORTB &= (~0x20);
   }
}

static inline void xxPlatform_SetInput(bool input)
{
   if(input)
   {
      DDRA = 0x00;
      DDRD = 0x00;
   }
   else
   {
      DDRA = 0xFF;
      DDRD = 0xFF;
   }
}


static inline void xxPlatform_SetData(unsigned int data)
{
   PORTD = (unsigned char)data;
   PORTA = (unsigned char)(data >> 0x08);
}


static inline unsigned int xxPlatform_GetData(void)
{
   return (((unsigned int)PINA << 0x08) | PIND);
}


static inline void xxPlatform_SetLED(bool val)
{
   if(val)
   {
      PORTC |= 0x80;
   }
   else
   {
      PORTC &= (~0x80);
   }
}
#endif
