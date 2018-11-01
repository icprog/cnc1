/******************************************************************************/
/*Filename:    Platform.h                                                     */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Declaration/definitions for platform independent functions.    */
/******************************************************************************/
#ifndef PLATFORM_H
#define PLATFORM_H
#define PLATFORM_UNKNOWN  (0x00)
#define PLATFORM_AVR      (0x01)
#define PLATFORM_GNULINUX (0x02)
#define PLATFORM_WINDOWS  (0x03)

#if PLATFORM_OS == PLATFORM_GNULINUX
#if _POSIX_C_SOURCE < 199309L
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif
#endif
#include "POSIX/POSIX.h"
#elif PLATFORM_OS == PLATFORM_WINDOWS
#include "Windows/Win64.h"
#else
#error Missing platform definitions for target platform
#endif
#endif
