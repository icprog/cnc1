/******************************************************************************/
/*Filename:    Platform.h                                                     */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Platform independant definitions.                              */
/******************************************************************************/
#ifndef PLATFORM_H
#define PLATFORM_H
#define PLATFORM_UNKNOWN (0x00)
#define PLATFORM_AVR     (0x01)
#define PLATFORM_LINUX   (0x02)
#define PLATFORM_WINDOWS (0x03)

#if PLATFORM_ARCH == PLATFORM_AVR
#include "AVR/AVR.h"
#else
#error Missing platform definitions for target platform
#endif
#endif
