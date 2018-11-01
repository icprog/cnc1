/******************************************************************************/
/*Filename:    IHex.c                                                         */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Function definitions for Intel HEX file format processing.     */
/******************************************************************************/
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "IHex.h"

/*Intel HEX record min/max valid sizes*/
#define MIN_RECORD_SIZE (0x0B)
#define MAX_RECORD_SIZE (0x0208 + sizeof('\n'))

/*Helper macro's*/
#define NTOH16(x) swap16((x))
#define NTOH32(x) swap32((x))

union size16
{
   unsigned char u8[sizeof(unsigned int)];
   unsigned int u16;
};

union size32
{
   unsigned char u8[sizeof(unsigned long)];
   unsigned long u32;
};

static bool convertHex(const char *, void *, unsigned char);
static unsigned int swap16(unsigned int);
static unsigned long swap32(unsigned long);


/* Open an Intel HEX file.
 *
 * INPUT : ihex - IHex_Session handle
 *         filename - name of file to open
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool IHex_Open(struct IHex_Session *restrict ihex,
               const char *restrict filename)
{
   ihex->file = fopen(filename, "r");

   if(ihex->file == NULL)
   {
      ihex->error = 0x00;
      return true;
   }

   ihex->startAddressSet = false;
   ihex->addressOffset = 0x00;
   return false;
}


/* Reset file (setting read position to 0).
 *
 * INPUT : ihex - IHex_Session handle
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool IHex_Reset(struct IHex_Session *restrict ihex)
{
   ihex->error = 0x01;
   return fseek(ihex->file, 0x00, SEEK_SET);
}


/* Get total of all data records in file.
 *
 * INPUT : ihex - IHex_Session handle
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool IHex_GetTotalSize(struct IHex_Session *restrict ihex,
                       unsigned long *restrict size)
{
   unsigned long address;
   unsigned char *data;
   unsigned char size2;

   if(IHex_Reset(ihex))
   {
      return true;
   }

   *size = 0x00;
   do
   {
      if(IHex_GetNextData(ihex, &address, &data, &size2))
      {
         return true;
      }
      *size += size2;
   } while(data != NULL);

   if(IHex_Reset(ihex))
   {
      return true;
   }

   return false;
}


/* Get Intel HEX file start address.
 *
 * INPUT : ihex - IHex_Session handle
 *
 * OUTPUT: address - location to store address
 *         [Return] - true if an error occurred, false otherwise
 */
bool IHex_GetStartAddress(struct IHex_Session *restrict ihex,
                          unsigned long *restrict address)
{
   if(ihex->startAddressSet)
   {
      *address = ihex->startAddress;
      return false;
   }

   ihex->error = 0x02;
   return true;
}


/* Retrieve data to write from next Intel HEX data record.
 *
 * INPUT : ihex - IHex_Session handle
 *
 * OUTPUT: address - location to store address
 *         data - location to store location of data
 *         size - location to store data size
 *         [Return] - true if an error occurred, false otherwise
 */
bool IHex_GetNextData(struct IHex_Session *restrict ihex,
                      unsigned long *restrict address,
                      unsigned char **restrict data,
                      unsigned char *restrict size)
{
   unsigned char dataSize;
   unsigned int dataAddress;
   unsigned char recordType;
   unsigned long check;
   unsigned char check1;
   unsigned char i;

   while(0x01)
   {
      ihex->buffer[MIN_RECORD_SIZE - 0x01] = '\n';
      ihex->buffer[MAX_RECORD_SIZE] = '\0';
      if(fgets((char *)ihex->buffer, MAX_RECORD_SIZE + 0x01,
               ihex->file) == NULL)
      {
         if(ferror(ihex->file))
         {
            clearerr(ihex->file);
         }

         ihex->error = 0x03;
         return true;
      }
      else if((ihex->buffer[MIN_RECORD_SIZE - 0x01] == '\n') ||
              (ihex->buffer[MAX_RECORD_SIZE] == '\n'))
      {
         ihex->error = 0x04;
         return true;
      }

      /*Verify fields*/
      ihex->error = 0x05;
      if(ihex->buffer[0x00] != ':')
      {
         return true;
      }
      else if(convertHex((char *)ihex->buffer + 0x01, &dataSize, 0x01))
      {
         return true;
      }
      else if(convertHex((char *)ihex->buffer + 0x03, &dataAddress, 0x02))
      {
         return true;
      }
      else if(convertHex((char *)ihex->buffer + 0x07, &recordType, 0x01))
      {
         return true;
      }

      dataAddress = NTOH16(dataAddress);

      /*Check checksum*/
      check = (unsigned long)dataSize + ((dataAddress & 0xFF00) >> 0x08) +
               (dataAddress & 0x00FF) + recordType;
      if(dataSize > 0x00)
      {
         dataSize--;
         for(i = 0x00; i <= dataSize; i++)
         {
            if(convertHex((char *)ihex->buffer + 0x09 + (i * 0x02), &check1,
                          0x01))
            {
               return true;
            }
            check += check1;
         }
         dataSize++;
      }

      check = ((~check) + 0x01);
      if((convertHex((char *)ihex->buffer + 0x09 + (dataSize * 0x02), &check1,
                     0x01)) ||
         ((check & 0xFF) != check1))
      {
         ihex->error = 0x06;
         return true;
      }

      /*Analyze data*/
      if(recordType == 0x00)
      {
         *address = (ihex->addressOffset + dataAddress);
         *data = ihex->buffer;
         *size = dataSize;

         if(convertHex((char *)ihex->buffer + 0x09, ihex->buffer, dataSize))
         {
            return true;
         }
         break;
      }
      /*End-of-File*/
      else if((recordType == 0x01) &&
              (!dataSize))
      {
         *address = 0x00;
         *data = NULL;
         *size = 0x00;
         break;
      }
      /*16-bit Address Offset*/
      else if((recordType == 0x02) &&
              (dataSize == 0x02))
      {
         if(convertHex((char *)ihex->buffer + 0x09, &ihex->addressOffset,
                       0x02))
         {
            return true;
         }
         ihex->addressOffset = NTOH16(ihex->addressOffset);
      }
      /*32-bit (x86 RM CS:IP) Start Address*/
      else if((recordType == 0x03) &&
              (dataSize == 0x04))
      {
         if(convertHex((char *)ihex->buffer + 0x09, &ihex->startAddress, 0x04))
         {
            return false;
         }
         ihex->startAddress = NTOH32(ihex->startAddress);
         ihex->startAddressSet = true;
      }
      /*32-bit Address Offset*/
      else if((recordType == 0x04) &&
              (dataSize == 0x04))
      {
         if(convertHex((char *)ihex->buffer + 0x09, &ihex->addressOffset,
                       0x04))
         {
            return true;
         }
         ihex->addressOffset = NTOH32(ihex->addressOffset);
      }
      /*32-bit Start Address*/
      else if((recordType == 0x05) &&
              (dataSize == 0x04))
      {
         if(convertHex((char *)ihex->buffer + 0x09, &ihex->startAddress, 0x04))
         {
            return true;
         }
         ihex->startAddress = NTOH32(ihex->startAddress);
         ihex->startAddressSet = true;
      }
      else
      {
         return true;
      }
   }

   return false;
}


/* Close an Intel HEX file.
 *
 * INPUT : ihex - An IHex_Session handle
 *
 * OUTPUT: None
 */
void IHex_Close(struct IHex_Session *restrict ihex)
{
   fclose(ihex->file);
}


/* Retrieve last IHex_Session error code.
 *
 * INPUT : ihex - An IHex_Session handle
 *
 * OUTPUT: [Return] - error code
 */
unsigned int IHex_GetError(struct IHex_Session *restrict ihex)
{
   return ihex->error;
}


/* Retrieve last IHex_Session error code string.
 *
 * INPUT : ihex - An IHex_Session handle
 *
 * OUTPUT: [Return] - error code string
 */
const char *IHex_GetErrorString(struct IHex_Session *restrict ihex)
{
   const char *lookup[] =
   {
      "Failed to open Intel HEX file",
      "Failed to reset Intel HEX file",
      "Start address not found",
      "Record read error",
      "Invalid record size",
      "Invalid record field",
      "Bad record checksum"
   };

   return lookup[ihex->error];
}


/* Convert hexadecimal string of characters to actual binary.
 *
 * INPUT : src - hexadecimal character string
 *         size - size of hexadecimal character string divided by 2
 *
 * OUTPUT: dest - binary representation output
 *         [Return] - true if an error occurred, false otherwise
 */
bool convertHex(const char *src, void *dest, unsigned char size)
{
   unsigned char i;
   unsigned char *buf = dest;
   bool high = true;
   const char *hexMap = "0123456789ABCDEF";

   for(; size > 0x00; src++)
   {
      for(i = 0x00; i <= strlen(hexMap); i++)
      {
         if(hexMap[i] == '\0')
         {
            return true;
         }
         else if(toupper(*src) == hexMap[i])
         {
            if(high)
            {
               *buf = (i << 0x04);
            }
            else
            {
               *buf |= i;
               size--;
               buf++;
            }
            high ^= true;
            break;
         }
      }
   }

   return false;
}


/* Swap incoming big-endian value to host endianess.
 *
 * INPUT : u16 - value to (possibly) swap endianess from
 * 
 * OUTPUT: [Return] - value with endianess (possibly) swapped
 */
unsigned int swap16(const unsigned int u16)
{
   union size16 result;
   
   result.u8[0x00] = (unsigned char)((u16 & 0xFF00) >> 0x08);
   result.u8[0x01] = (unsigned char)(u16 & 0x00FF);

   return (result.u16 & 0xFFFF);
}


/* Swap incoming big-endian value to host endianess.
 *
 * INPUT : u32 - value to (possibly) swap endianess from
 * 
 * OUTPUT: [Return] - value with endianess (possibly) swapped
 */
unsigned long swap32(const unsigned long u32)
{
   union size32 result;
   
   result.u8[0x00] = (unsigned char)((u32 & 0xFF000000UL) >> 0x18);
   result.u8[0x01] = (unsigned char)((u32 & 0x00FF0000UL) >> 0x10);
   result.u8[0x02] = (unsigned char)((u32 & 0x0000FF00UL) >> 0x08);
   result.u8[0x03] = (unsigned char)(u32 & 0x000000FFUL);

   return (result.u32 & 0xFFFFFFFFUL);
}
