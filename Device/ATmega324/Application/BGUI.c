/******************************************************************************/
/*Filename:    BGUI.c                                                         */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Basic GUI library definitions.                                 */
/******************************************************************************/
#include <string.h>
#include "SSD1289.h"
#include "BGUI.h"

/*Max BGUI active objects*/
#ifndef BGUI_MAX_GUI_OBJECTS
#define BGUI_MAX_GUI_OBJECTS (0x06)
#endif

struct GUIObject
{
   unsigned char id;
   unsigned int x;
   unsigned char y;
   unsigned int xEnd;
   unsigned int yEnd;
};

/*Static library (global/one session) variables*/
static void (*eventHandler)(unsigned char, unsigned char);
static unsigned char objectCount;
static struct GUIObject objects[BGUI_MAX_GUI_OBJECTS];
static unsigned char activeObject;


/* Open default BGUI session.
 *
 * INPUT : [None]
 *
 * OUTPUT: [Return] - false if session opened, true otherwise
 */
bool BGUI_Open(void (*cb)(unsigned char, unsigned char))
{
   eventHandler = cb;
   objectCount = 0x00;
   return false;
}


/* Close default BGUI session.
 *
 * INPUT : [None]
 *
 * OUTPUT: [None]
 */
void BGUI_Close(void)
{

}


/* Create a BGUI button object.
 *
 * INPUT : name - name of button
 *         x - x-coordinate to place button
 *         y - y-coordinate to place button
 *         width - width of button
 *         height - height of button
 *         id - id of button
 *
 * OUTPUT: [None]
 */
bool BGUI_CreateButton(const char *restrict name, unsigned int x,
                       unsigned char y, const unsigned int width,
                       const unsigned char height, const unsigned char id)
{
   struct GUIObject obj;

   /*TOOD: Round off corners of button*/
   SSD1289_FillRect(x, y, width, height, 0x001F);

   /*Write button name in center of button*/
   x += ((width - (strlen(name) * SSD1289_FONT_WIDTH)) / 0x02);
   y += ((height - SSD1289_FONT_HEIGHT) / 0x02);
   SSD1289_WriteString(x, y, name, 0xFFFF, 0x001F);

   /*Insert button into list*/
   obj.id = id;
   obj.x = x;
   obj.y = y;
   obj.xEnd = (x + width);
   obj.yEnd = ((unsigned int)y + height);
   objects[objectCount++] = obj;
   return false;
}


/* Destroy/remove a BGUI button object.
 *
 * INPUT : id - id of button
 *
 * OUTPUT: [None]
 */
void BGUI_DestroyButton(unsigned char id)
{
   //TODO: remove (or mark) obj from list
}


/* Register BGUI hit/touch/click.
 *
 * INPUT : x - x-coordinate of hit
 *         y - y-coordinate of hit
 *
 * OUTPUT: [None]
 */
void BGUI_Hit(const unsigned int x, const unsigned char y)
{
   unsigned char i;

   /*Check all objects to determine which was hit (i.e. hit-testing)*/
   for(i = 0x00; i < objectCount; i++)
   {
      if(((x >= objects[i].x) && (x <= objects[i].xEnd)) &&
         ((y >= objects[i].y) && (y <= objects[i].yEnd)))
      {
         /*Mark object as active and send button down event*/
         activeObject = i;
         eventHandler(objects[activeObject].id, BGUI_BTN_DOWN);
         break;
      }
   }
}


/* Register BGUI hit/touch/click release
 *
 * INPUT : [None]
 *
 * OUTPUT: [None]
 */
void BGUI_Release(void)
{
   /*Send button released event to active object*/
   eventHandler(objects[activeObject].id, BGUI_BTN_UP);
}
