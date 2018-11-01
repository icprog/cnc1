/******************************************************************************/
/*Filename:    XPT2046.h                                                      */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Library for XPT2046 touch controller.                          */
/******************************************************************************/
#ifndef XPT2046_H
#define XPT2046_H
#include <stdbool.h>

bool XPT2046_Open(void);
void XPT2046_Close(void);
void XPT2046_EnableINT(const bool, void (*)(void));
bool XPT2046_GetXY(unsigned int *restrict, unsigned int *restrict);

#endif
