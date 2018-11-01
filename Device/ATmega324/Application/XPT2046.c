/******************************************************************************/
/*Filename:    XPT2046.c                                                      */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Definitions for XPT2046 touch controller library.              */
/******************************************************************************/
#include <stdbool.h>
#include "Platform.h"
#include "XPT2046.h"

static unsigned int getConversion(unsigned char);


/* Open default XPT2046 session.
 *
 * INPUT : [None]
 *
 * OUTPUT: [Return] - false if session opened, true otherwise
 */
bool XPT2046_Open(void)
{
   /*Do dummy conversion to ensure INT pin is enabled*/
   XPT2046_SET_INPUT(true);
   getConversion(0x90);
   XPT2046_SET_INPUT(false);
   return false;
}


/* Close default XPT2046 session.
 *
 * INPUT : [None]
 *
 * OUTPUT: [None]
 */
void XPT2046_Close(void)
{

}


/* Enable XPT2046 interrupt (active when touch active).
 *
 * INPUT : bool - true to enable interrupt, false to disable
 *         cb - callback when interrupt is activated
 *
 * OUTPUT: [None]
 */
void XPT2046_EnableINT(const bool enable, void (*cb)(void))
{
   XPT2046_ENABLE_IRQ(enable, cb);
}


/* Get XY coordinates of valid touch.
 *
 * INPUT : [None]
 *
 * OUTPUT: x - X coordinate of touch (relative to size)
 *         y - Y coordinate of touch (relative to size)
 *         [Return] - false if touch coordinates retrieved, true otherwise
 */
bool XPT2046_GetXY(unsigned int *restrict x, unsigned int *restrict y)
{
   unsigned char i;
   unsigned int samP[0x0A];
   unsigned int samX[0x0A];
   unsigned int samY[0x0A];
   unsigned char minX = 0xFF;
   unsigned char maxX = 0xFF;
   unsigned char samples = 0x00;
   unsigned long avgX = 0x00;
   unsigned long avgY = 0x00;

   /*Get samples (as quickly as possible to get accurate touch)*/
   XPT2046_SET_INPUT(true);
   getConversion(0xD3);
   for(i = 0x00; i < 0x0A; i++)
   {
      /*Get pressure measurement*/
      samP[i] = getConversion(0xB3);
      /*Get Y coordinate*/
      samY[i] = getConversion(0xD3);
      /*Get X coordinate*/
      samX[i] = getConversion(0x93);
   }
   getConversion(0x90);
   XPT2046_SET_INPUT(false);

   /*Exclude min and max valid samples and average the rest*/
   for(i = 0x00; i < 0x0A; i++)
   {
      if((samP[i] > 0x64) &&
         ((samX[i] > 0x012C) && (samX[i] < 0x0ED8)) &&
         ((samY[i] > 0x01F4) && (samY[i] < 0x0DAC)))
      {
         if((minX == 0xFF) ||
            (samX[i] < samX[minX]))
         {
            minX = i;
         }
         else if((maxX == 0xFF) ||
                 (samX[i] > samX[maxX]))
         {
            maxX = i;
         }

         avgX += samX[i];
         avgY += samY[i];
         samples++;
      }
   }

   /*Minimum 6 valid samples for a good touch*/
   if(samples < 0x06)
   {
      return true;
   }

   samples -= 0x02;
   avgX -= (samX[minX] + samX[maxX]);
   avgY -= (samY[minX] + samY[maxX]);
   *x = (((avgX / samples) - 0x012C) / (0x0DAC / 0x0140));
   *y = (((avgY / samples) - 0x01F4) / (0x0BB8 / 0xF0));
   return false;
}


/* Get ADC conversion from XPT2046.
 *
 * INPUT : control - control byte specifying conversion parameters
 *
 * OUTPUT: [Return] - conversion result
 */
unsigned int getConversion(unsigned char control)
{
   unsigned char i;
   unsigned int result = 0x00;

   XPT2046_SET_DCLK(false);
   XPT2046_SET_CS(false);
   Platform_SleepNS(0x64);
   /*Send control byte*/
   for(i = 0x00; i < 0x08; i++)
   {
      XPT2046_SET_IN(control & 0x80);
      control <<= 0x01;
      Platform_SleepNS(0xC8);
      XPT2046_SET_DCLK(true);
      Platform_SleepNS(0xC8);
      XPT2046_SET_DCLK(false);
   }
   Platform_SleepNS(0xC8);
   /*Wait for BUSY HIGH (touch latched)*/
   while(!XPT2046_GET_BUSY());
   /*Recieve conversion result*/
   for(i = 0x00; i < 0x0C; i++)
   {
      XPT2046_SET_DCLK(true);
      Platform_SleepNS(0xC8);
      XPT2046_SET_DCLK(false);
      Platform_SleepNS(0xC8);
      result <<= 0x01;
      result |= XPT2046_GET_OUT();
   }
   XPT2046_SET_CS(true);
   Platform_SleepNS(0xC8);

   return result;
}
