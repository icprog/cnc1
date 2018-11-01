/******************************************************************************/
/*Filename:    Win64.h                                                        */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Declaration/definitions for Windows platform independent       */
/*             functions.                                                     */
/******************************************************************************/
#ifndef WINDOWS_H
#define WINDOWS_H
#include <windows.h>

static inline void Platform_Sleep(unsigned int s)
{
   Sleep(s * 0x03E8);
}


static inline void Platform_SleepMS(unsigned int ms)
{
   Sleep(ms);
}


static inline void Platform_SleepUS(unsigned int us)
{
   Sleep(0x01);
}


static inline void Platform_SleepNS(unsigned int ns)
{
   Sleep(0x01);
}


static inline void Platform_RepeatNS(unsigned int *last, unsigned int ns)
{
   /*At this resolution (<1000ns) do the full sleep*/
   Platform_SleepNS(ns);
/*
   if(*last == 0x00)
   {
      last = currentTime();
   }
   else
   {
      if((currentTime() - *last) < ns)
      {
         delay(diff);
      }
      last = currentTime();
   }
*/
}

#endif
