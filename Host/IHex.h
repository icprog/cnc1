/******************************************************************************/
/*Filename:    IHex.h                                                         */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Intel HEX file processing utilities library.                   */
/******************************************************************************/
#ifndef IHEX_H
#define IHEX_H
#include <stdbool.h>
#include <stdio.h>

#define MAX_RECORD_SIZE (0x0208 + sizeof('\n'))

struct IHex_Session
{
   FILE *file;
   bool startAddressSet;
   unsigned long startAddress;
   unsigned long addressOffset;
   unsigned char buffer[MAX_RECORD_SIZE + 0x01];
   unsigned int error;
};


bool IHex_Open(struct IHex_Session *restrict, const char *restrict);
void IHex_Close(struct IHex_Session *restrict);
bool IHex_Reset(struct IHex_Session *restrict);
bool IHex_GetTotalSize(struct IHex_Session *restrict, unsigned long *restrict);
bool IHex_GetStartAddress(struct IHex_Session *restrict,
                          unsigned long *restrict);
bool IHex_GetNextData(struct IHex_Session *restrict, unsigned long *restrict,
                      unsigned char **restrict, unsigned char *restrict);
unsigned int IHex_GetError(struct IHex_Session *restrict);
const char *IHex_GetErrorString(struct IHex_Session *restrict);

#undef MAX_RECORD_SIZE
#endif
