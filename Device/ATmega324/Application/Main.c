/******************************************************************************/
/*Filename:    Main.c                                                         */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: CNC master controller. Controls the LCD, Touchscreen and slave */
/*             CNC controller (which subsequently controls the CNC axis and   */
/*             drill motors).                                                 */
/******************************************************************************/
#include <stdbool.h>
#include <stdlib.h>
#include "Platform.h"
#include "BootExport.h"
#include "XPT2046.h"
#include "SSD1289.h"
#include "BGUI.h"

/*General Flags*/
#define FLAG_TWI_INT   (0x01)
#define FLAG_TP_INT    (0x02)
#define FLAG_TP_DOWN   (0x04)
#define FLAG_TIMER_INT (0x08)

/*GUI Button ID's*/
#define BTN_RAISE (0x00)
#define BTN_LOWER (0x01)
#define BTN_LEFT  (0x02)
#define BTN_RIGHT (0x03)
#define BTN_UP    (0x04)
#define BTN_DOWN  (0x05)

void TWIINT(void);
void touchINT(void);
void timerINT(void);
void guiEvents(unsigned char, unsigned char);

/*Global variable*/
volatile unsigned char flags = 0x00;


/* Process memory read commands.
 *
 * INPUT : addr - address for read
 *         data - output buffer for read data
 *         size - size of data to be read
 *
 * OUTPUT: [Return] - true if error occurred, false otherwise
 */
bool memRead(unsigned long long addr, void *data, unsigned char size)
{
   return false;
}


/* Process memory write commands.
 *
 * INPUT : addr - address for write
 *         data - input buffer for write data
 *         size - size of data to be written
 *
 * OUTPUT: [Return] - true if error occurred, false otherwise
 */
bool memWrite(unsigned long long addr, void *data, unsigned char size)
{
   return false;
}


/* TWI interrupt callback.
 *
 * INPUT : [None]
 *
 * OUTPUT: [None]
 */
void TWIINT(void)
{
   flags |= FLAG_TWI_INT;
}


/* Touch panel interrupt callback.
 *
 * INPUT : [None]
 *
 * OUTPUT: [None]
 */
void touchINT(void)
{
   flags |= FLAG_TP_INT;
}


/* Timer interrupt callback.
 *
 * INPUT : [None]
 *
 * OUTPUT: [None]
 */
void timerINT(void)
{
   flags |= FLAG_TIMER_INT;
}


/* GUI events callback.
 *
 * INPUT : id - id of GUI object
 *         ev - GUI object event
 *
 * OUTPUT: [None]
 */
void guiEvents(unsigned char id, unsigned char ev)
{
   if(ev == BGUI_BTN_DOWN)
   {
      SSD1289_WriteString(0x00, 0x00, "Hit", 0xFFFF, 0x00);
   }
}


int main(void)
{
   struct BCP_Session bcp;
   unsigned int tpX;
   unsigned int tpY;

   /*Initialize libraries*/
   Platform_Open();
   XPT2046_Open();
   SSD1289_Open();
   BGUI_Open(guiEvents);
   BCP_Open(&bcp);

   /*Create GUI*/
   SSD1289_FillRect(0x00, 0x00, 0x0140, 0xF0, 0x00);
   BGUI_CreateButton("Raise", 0x20, 0xB0, 0x40, 0x20, BTN_RAISE);
   BGUI_CreateButton("Lower", 0x20, 0x80, 0x40, 0x20, BTN_LOWER);
   BGUI_CreateButton("Left", 0x80, 0x50, 0x40, 0x20, BTN_LEFT);
   BGUI_CreateButton("Right", 0xE0, 0x50, 0x40, 0x20, BTN_RIGHT);
   BGUI_CreateButton("Up", 0xB0, 0x80, 0x40, 0x20, BTN_UP);
   BGUI_CreateButton("Down", 0xB0, 0x20, 0x40, 0x20, BTN_DOWN);

   /*Setup interrupt sources and enable interrupts*/
   XPT2046_EnableINT(true, touchINT);
   BootExport_EnableTWIINT2(true, TWIINT);
   Platform_EnableInterrupts(true);

   while(0x01)
   {
      /*Check for BCP request*/
      if(flags & FLAG_TWI_INT)
      {
         BCP_HandleRequest(&bcp, memRead, memWrite);
         flags &= (~(FLAG_TWI_INT));
      }

      /*Check for touch panel touch*/
      if(flags & FLAG_TP_INT)
      {
         if(!XPT2046_GetXY(&tpX, &tpY))
         {
            XPT2046_EnableINT(false, NULL);
            Platform_SetTimer(0x14, timerINT);
         }

         flags &= (~(FLAG_TP_INT));
      }

      /*Check that touch panel touch is still present*/
      if(flags & FLAG_TIMER_INT)
      {
         if(XPT2046_GetXY(&tpX, &tpY))
         {
            if(flags & FLAG_TP_DOWN)
            {
               BGUI_Release();
               flags &= (~(FLAG_TP_DOWN));
            }

            XPT2046_EnableINT(true, touchINT);
         }
         else
         {
            if(!(flags & FLAG_TP_DOWN))
            {
               BGUI_Hit(tpX, tpY);
               flags |= FLAG_TP_DOWN;
            }

            Platform_SetTimer(0xC8, timerINT);
         }

         flags &= (~(FLAG_TIMER_INT));
      }
   }

   return 0x00;
}
