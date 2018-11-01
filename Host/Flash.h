/******************************************************************************/
/*Filename:    Flash.h                                                        */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Device flash writing/verifiying library utilities.             */
/******************************************************************************/
#ifndef FLASH_H
#define FLASH_H
#include <stdbool.h>
#include "IHex.h"
#include "BCP.h"

struct Flash_Session
{
   struct IHex_Session file;
   struct BCP_Session *bcp;
   unsigned int size;
   unsigned int error;
};


bool Flash_Open(struct Flash_Session *restrict, struct BCP_Session *restrict,
                const char *restrict);
void Flash_Close(struct Flash_Session *restrict);
bool Flash_GetSize(struct Flash_Session *restrict, unsigned char *,
                   unsigned int *);
bool Flash_Write(struct Flash_Session *restrict, void (*)(),
                 const unsigned char);
bool Flash_Verify(struct Flash_Session *restrict, void (*)(),
                  const unsigned char);
unsigned int Flash_GetError(struct Flash_Session *restrict);
const char *Flash_GetErrorString(struct Flash_Session *restrict);

#endif
