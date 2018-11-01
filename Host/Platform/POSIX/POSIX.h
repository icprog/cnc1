/******************************************************************************/
/*Filename:    POSIX.h                                                        */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Declaration/definitions for a POSIX-compatible platform        */
/*             independant functions.                                         */
/******************************************************************************/
#ifndef POSIX_H
#define POSIX_H
#include <sys/time.h>
#include <unistd.h>

static inline void Platform_Sleep(unsigned int s)
{
   sleep(s);
}


static inline void Platform_SleepMS(unsigned int ms)
{
   struct timespec ts = {.tv_sec = 0x00, .tv_nsec = (ms * 0x000F4240)};

   nanosleep(&ts, NULL);
}


static inline void Platform_SleepUS(unsigned int us)
{
   struct timespec ts = {.tv_sec = 0x00, .tv_nsec = (us * 0x03E8)};

   nanosleep(&ts, NULL);
}


static inline void Platform_SleepNS(unsigned int ns)
{
   struct timespec ts = {.tv_sec = 0x00, .tv_nsec = ns};

   nanosleep(&ts, NULL);
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

