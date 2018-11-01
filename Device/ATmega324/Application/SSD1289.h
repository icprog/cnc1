/******************************************************************************/
/*Filename:    SSD1289.h                                                      */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Library for interfacing with an SSD1289 LCD controller. Library*/
/*             is configured to use LCD in horizontal orientation (320x240)   */
/*             communicating using the 16-bit 8080 interface.                 */
/******************************************************************************/
#ifndef SSD1289_H
#define SSD1289_H
#include <stdbool.h>

#define SSD1289_FONT_WIDTH  (0x08)
#define SSD1289_FONT_HEIGHT (0x0C)


bool SSD1289_Open(void);
void SSD1289_Close(void);
void SSD1289_SetPixel(const unsigned int, const unsigned char,
                      const unsigned int);
unsigned int SSD1289_GetPixel(const unsigned int, const unsigned char);
void SSD1289_FillRect(const unsigned int, const unsigned char,
                      const unsigned int, const unsigned char,
                      const unsigned int);
void SSD1289_WriteString(const unsigned int, const unsigned char,
                         const char *restrict, const unsigned int,
                         const unsigned int);

#endif
