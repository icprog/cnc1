/******************************************************************************/
/*Filename:    SSD1289.c                                                      */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Definitions for interfacing with an SSD1289 LCD controller.    */
/******************************************************************************/
#include <stdbool.h>
#include <string.h>
#include "Platform.h"
#include "CharMap.h"
#include "SSD1289.h"
#if PLATFORM_ARCH == PLATFORM_AVR
#include <avr/pgmspace.h>
#endif

/*Check that interface pins are assigned*/
#if (!defined(SSD1289_SET_CS) || \
     !defined(SSD1289_SET_RS) || \
     !defined(SSD1289_SET_RD) || \
     !defined(SSD1289_SET_WR) || \
     !defined(SSD1289_SET_DATA_INPUT) || \
     !defined(SSD1289_SET_DATA16) || \
     !defined(SSD1289_GET_DATA16))
#error SSD1289: Interface macro functions not defined
#endif

static void setRegister(const unsigned char);
static void setRegisterValue(const unsigned char, const unsigned int);
static unsigned int readValue(void);
static void writeValue(const unsigned int);
static void writeData(const unsigned int);


/* Open default SSD1289 session.
 *
 * INPUT : [None]
 *
 * OUTPUT: [Return] - false if session opened, true otherwise
 */
bool SSD1289_Open(void)
{
   /*Configure display to usable state*/
   SSD1289_SET_CS(true);
   /*Set internal display on (external off), GND display drivers*/
   setRegisterValue(0x07, 0x21);
   /*Enable display oscillator*/
   setRegisterValue(0x00, 0x01);
   /*Start display driver charge*/
   setRegisterValue(0x07, 0x23);
   /*Exit sleep mode*/
   setRegisterValue(0x10, 0x00);
   /*Delay to build voltage and stabalize*/
   Platform_SleepMS(0x1E);
   /*Enable external display, enable 1st screen vertical scroll*/
   setRegisterValue(0x07, 0x0233);
   /*Set 65K colour mode, RAM display, vertical/horizontal address increment*/
   setRegisterValue(0x11, 0x6078);
   /*Set standard wave-form drive (no front/back porch blanking for RAM data)*/
   setRegisterValue(0x02, 0x0600);
   /*Set interlaced, 320 scan lines, normally black planel (Reversed)*/
   setRegisterValue(0x01, 0x2B3F);
   /*Set gamma adjustment (low RGB brighter, higher less bright)*/
   setRegisterValue(0x30,0x0707);
   setRegisterValue(0x31,0x0204);
   setRegisterValue(0x32,0x0204);
   setRegisterValue(0x33,0x0502);
   setRegisterValue(0x34,0x0507);
   setRegisterValue(0x35,0x0204);
   setRegisterValue(0x36,0x0204);
   setRegisterValue(0x37,0x0502);
   setRegisterValue(0x3A,0x0302);
   setRegisterValue(0x3B,0x0302);
   return false;
}


/* Close default SSD1289 session.
 *
 * INPUT : [None]
 *
 * OUTPUT: [None]
 */
void SSD1289_Close(void)
{

}


/* Set pixel at (x, y) location.
 *
 * INPUT : x - X coordinate
 *         y - Y coordinate
 *         colour - pixel colour
 *
 * OUTPUT: [None]
 */
void SSD1289_SetPixel(const unsigned int x, const unsigned char y,
                      const unsigned int colour)
{
   setRegisterValue(0x4F, x);
   setRegisterValue(0x4E, y);
   setRegister(0x22);
   writeValue(colour);
}


/* Get pixel at location.
 *
 * INPUT : x - X coordinate
 *         y - Y coordinate
 *
 * OUTPUT: [Return] - pixel colour
 */
unsigned int SSD1289_GetPixel(const unsigned int x, const unsigned char y)
{
   setRegisterValue(0x4F, x);
   setRegisterValue(0x4E, y);
   setRegister(0x22);
   return readValue();
}


/* Fill rectangle at (x, y) location of size (width, height).
 *
 * INPUT : x - X coordinate
 *         y - Y coordinate
 *         width - width of rectangle
 *         height - height of rectangle
 *         colour - colour of rectangle
 *
 * OUTPUT: [None]
 */
void SSD1289_FillRect(const unsigned int x, const unsigned char y,
                      const unsigned int width, const unsigned char height,
                      const unsigned int colour)
{
   unsigned long total = (unsigned long)width * height;

   /*Set start (x, y) and wrap (width, height)*/
   setRegisterValue(0x4F, x);
   setRegisterValue(0x4E, y);
   setRegisterValue(0x45, x);
   setRegisterValue(0x46, x + (width - 0x01));
   setRegisterValue(0x44, (((unsigned int)y + (height - 0x01)) << 0x08) | y);
   setRegister(0x22);

   /*Write pixel data*/
   while(total--)
   {
      writeValue(colour);
   }

   /*Restore default wrap*/
   setRegisterValue(0x45, 0x00);
   setRegisterValue(0x46, 0x013F);
   setRegisterValue(0x44, 0xEF << 0x08);
}


/* Write/display string representation.
 *
 * INPUT : x - X coordinate
 *         y - Y coordinate
 *         str - string to display
 *         forecolour - forecolour of string
 *         backcolour - backcolour of string
 *
 * OUTPUT: [None]
 */
void SSD1289_WriteString(const unsigned int x, const unsigned char y,
                         const char *restrict str,
                         const unsigned int foreColour,
                         const unsigned int backColour)
{
   unsigned char i;
   unsigned char charLine;
   unsigned char len = strlen(str);
   unsigned char line = 0x00;
   const char *c = str;

   /*Set start (x, y) and wrap (width, height)*/
   setRegisterValue(0x4F, x);
   setRegisterValue(0x4E, y);
   setRegisterValue(0x45, x);
   setRegisterValue(0x46, x + ((len * 0x08) - 0x01));
   setRegisterValue(0x44, (((unsigned int)y + (0x0C - 0x01)) << 0x08) | y);
   setRegister(0x22);

   /*Write character pixel data*/
   while(line != 0x0C)
   {
#if PLATFORM_ARCH == PLATFORM_AVR
      charLine = pgm_read_byte(&CharMap_Bitmap[((*c) - 0x20) + (line * 0x60)]);
#else
      charLine = CharMap_Bitmap[((*c) - 0x20) + (line * 0x60)];
#endif
      for(i = 0x00; i < 0x08; i++)
      {
         if(charLine & (0x80 >> i))
         {
            writeValue(foreColour);
         }
         else
         {
            writeValue(backColour);
         }
      }

      c++;
      if(*c == '\0')
      {
         c = str;
         line++;
      }
   }

   /*Restore default wrap*/
   setRegisterValue(0x45, 0x00);
   setRegisterValue(0x46, 0x013F);
   setRegisterValue(0x44, 0xEF << 0x08);
}


/* Set display register.
 *
 * INPUT : reg - register index
 *
 * OUTPUT: [None]
 */
void setRegister(const unsigned char reg)
{
   SSD1289_SET_RS(false);
   writeData(reg);
}


/* Set display register and write value in register.
 *
 * INPUT : reg - register index
 *         val - value to write to register
 *
 * OUTPUT: [None]
 */
void setRegisterValue(const unsigned char reg, const unsigned int val)
{
   setRegister(reg);
   writeValue(val);
}


/* Read value from current register.
 *
 * INPUT : [None]
 *
 * OUTPUT: [Return] - current register value
 */
unsigned int readValue(void)
{
   unsigned int data;
   unsigned int last = 0x00;

   SSD1289_SET_RS(true);
   SSD1289_SET_RD(true);
   SSD1289_SET_WR(true);
   SSD1289_SET_DATA_INPUT(true);
   Platform_RepeatNS(&last, 0x1F4);
   /*Dummy read*/
   SSD1289_SET_CS(false);
   SSD1289_SET_RD(false);
   Platform_SleepNS(0xFA);
   SSD1289_GET_DATA16();
   Platform_SleepNS(0xFA);
   SSD1289_SET_CS(true);
   SSD1289_SET_RD(true);
   /*Actual read*/
   Platform_SleepNS(0x01F4);
   SSD1289_SET_CS(false);
   SSD1289_SET_RD(false);
   Platform_SleepNS(0xFA);
   data = SSD1289_GET_DATA16();
   Platform_SleepNS(0xFA);
   SSD1289_SET_CS(true);
   SSD1289_SET_DATA_INPUT(false);

   return data;
}


/* Write value to current register.
 *
 * INPUT : val - value to write to current register
 *
 * OUTPUT: [None]
 */
void writeValue(const unsigned int val)
{
   SSD1289_SET_RS(true);
   writeData(val);
}


/* Write 16-bit data to display.
 *
 * INPUT : data - data to write
 *
 * OUTPUT: [None]
 */
void writeData(const unsigned int data)
{
   static unsigned int last = 0x00;

   SSD1289_SET_RD(true);
   SSD1289_SET_WR(true);
   Platform_RepeatNS(&last, 0x32);
   SSD1289_SET_CS(false);
   SSD1289_SET_DATA16(data);
   SSD1289_SET_WR(false);
   Platform_SleepNS(0x32);
   SSD1289_SET_CS(true);
}
