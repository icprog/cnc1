/******************************************************************************/
/*Filename:    Flash.c                                                        */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Definitions for device flash writing/verifiying library utility*/
/*             functions.                                                     */
/******************************************************************************/
#include <stdbool.h>
#include <string.h>
#include "BCP.h"
#include "IHex.h"
#include "Flash.h"

static bool writeVerify(struct Flash_Session *restrict, void (*)(),
                        const unsigned char, const bool);


/* Initialize flash library interface.
 *
 * INPUT : flash - Flash_Session handle
 *         bcp - BCP_Session handle
 *         filename - name of Intel HEX file to open
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool Flash_Open(struct Flash_Session *restrict flash,
                struct BCP_Session *restrict bcp,
                const char *restrict filename)
{
   unsigned char bcpBuffer[0x08];
   unsigned long size;
   bool ret = true;

   /*Open hex file to be flashed to device*/
   if(IHex_Open(&flash->file, filename))
   {
      flash->error = 0x00;
      return true;
   }

   /*Check device is in flash mode*/
   memset(bcpBuffer, 0x00, 0x08);
   if((BCP_SetAddress(bcp, 0xFFFFFFFFFFFFFFF8ULL)) ||
      (BCP_ReadMemory(bcp, bcpBuffer, 0x08)) ||
      (memcmp(bcpBuffer, "BOOTLOAD", 0x08) != 0x00))
   {
      flash->error = 0x01;
      goto closeFile;
   }

   /*Unlock flash to allow for write*/
   bcpBuffer[0x00] = 0x01;
   if((BCP_SetAddress(bcp, 0x010000ACE0000010ULL)) ||
      (BCP_WriteMemory(bcp, bcpBuffer, 0x01)))
   {
      flash->error = 0x02;
      goto closeFile;
   }

   /*Get total hex file size*/
   if(IHex_GetTotalSize(&flash->file, &size))
   {
      flash->error = 0x03;
      goto closeFile;
   }
   
   flash->bcp = bcp;
   flash->size = size;
   ret = false;
closeFile:
   return ret;
}


/* Close flash library interface.
 *
 * INPUT : flash - Flash_Session handle
 *
 * OUTPUT: [None]
 */
void Flash_Close(struct Flash_Session *restrict flash)
{
   IHex_Close(&flash->file);
}


/* Retrieve error code for Flash_Session.
 *
 * INPUT : flash - Flash_Session handle
 *
 * OUTPUT: [Return] - error code
 */
unsigned int Flash_GetError(struct Flash_Session *restrict flash)
{
   return flash->error;
}


/* Retrieve error code string for Flash_Session.
 *
 * INPUT : flash - Flash_Session handle
 *
 * OUTPUT: [Return] - error code string
 */
const char *Flash_GetErrorString(struct Flash_Session *restrict flash)
{
   const char *lookup[] =
   {
      "Unable to open Intel Hex file",
      "Device not in flash mode",
      "Unable to unlock device flash",
      "Unable to retrieve hex file size",
      "Unable to retrieve pages written",
      "Failed setup for write/verify",
      "Failed to commit flash write",
      "Device Read/Write/Address error",
      "Device verification failed, byte mismatch"
   };

   return lookup[flash->error];
}


/* Get total size flashed.
 *
 * INPUT : flash - Flash_Session handle
 *         pages - size flashed in device pages
 *         bytes - size flashed in bytes
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool Flash_GetSize(struct Flash_Session *restrict flash, unsigned char *pages,
                   unsigned int *bytes)
{
   unsigned char bcpBuffer[0x08];

   /*Attempt to get page write count*/
   bcpBuffer[0x00] = 0x00;
   if((BCP_SetAddress(flash->bcp, 0xFFFFFFFFFFFFFFF7ULL)) ||
      (BCP_ReadMemory(flash->bcp, bcpBuffer, 0x01)))
   {
      flash->error = 0x04;
      return true;
   }

   *pages = bcpBuffer[0x00];
   *bytes = flash->size;
   return false;
}


/* Flash device.
 *
 * INPUT : flash - Flash_Session handle
 *         cb - callback to progress/update function
 *         rate - rate to callback update function (in percent)
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool Flash_Write(struct Flash_Session *restrict flash, void (*cb)(),
                 const unsigned char rate)
{
   return writeVerify(flash, cb, rate, false);
}


/* Verify device flash matches file.
 *
 * INPUT : flash - Flash_Session handle
 *         cb - callback to progress/update function
 *         rate - rate to callback update function (in percent)
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool Flash_Verify(struct Flash_Session *restrict flash, void (*cb)(),
                  const unsigned char rate)
{
   return writeVerify(flash, cb, rate, true);
}


/* Write input file to device or verify device flash memory contents are
 * identical to input file.
 *
 * INPUT : flash - Flash_Session handle
 *         update - callback update function
 *         rate - rate to call update function (in percent)
 *         verify - true to verify flash, false to write flash
 *
 * OUTPUT: [return] - true if success, false otherwise
 */
bool writeVerify(struct Flash_Session *restrict flash, void (*update)(),
                 const unsigned char rate, const bool verify)
{
   unsigned long address;
   unsigned char dataSize;
   unsigned char *data;
   unsigned char sent;
   unsigned char bcpBuffer[0x08];
   unsigned long lastAddress = 0x00;
   unsigned long rwSize = 0x00;
   unsigned char updates = 0x00;

   /*Setup*/
   if((IHex_Reset(&flash->file)) ||
      (BCP_SetAddress(flash->bcp, 0x00)) ||
      (BCP_SetFlags(flash->bcp, FLAG_ADDR_INC)))
   {
      flash->error = 0x05;
      return true;
   }

   /*Flash full Intel HEX file*/
   while(0x01)
   {
      if(IHex_GetNextData(&flash->file, &address, &data, &dataSize))
      {
         return true;
      }

      if(data == NULL)
      {
         if(!verify)
         {
            /*Lock flash to ensure all previous writes are committed*/
            bcpBuffer[0x00] = 0x00;
            if((BCP_SetAddress(flash->bcp, 0x010000ACE0000010ULL)) ||
               (BCP_WriteMemory(flash->bcp, bcpBuffer, 0x01)))
            {
               flash->error = 0x07;
               return true;
            }
         }

         return false;
      }
      else if(dataSize == 0x00)
      {
         continue;
      }

      if(address != lastAddress)
      {
         if(BCP_SetAddress(flash->bcp, address))
         {
            flash->error = 0x07;
            return true;
         }
         lastAddress = address;
      }

      lastAddress += dataSize;
      while(dataSize)
      {
         sent = (dataSize > 0x08) ? 0x08 : dataSize;
         if(verify)
         {
            if(BCP_ReadMemory(flash->bcp, bcpBuffer, sent))
            {
               flash->error = 0x07;
               return true;
            }

            if(memcmp(bcpBuffer, data, sent))
            {
               flash->error = 0x08;
               return true;
            }
         }
         else
         {
            if(BCP_WriteMemory(flash->bcp, data, sent))
            {
               flash->error = 0x07;
               return true;
            }
         }

         dataSize -= sent;
         data += sent;
         rwSize += sent;

         if(rate != 0x00)
         {
            /*Callback progress update function*/
            while(updates != (((rwSize * 0x64) / flash->size) / rate))
            {
               update();
               updates++;
            }
         }
      }
   }
}
