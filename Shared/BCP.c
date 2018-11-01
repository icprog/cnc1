/******************************************************************************/
/*Filename:    BCP.c                                                          */
/*Project:     Basic Control Protocol                                         */
/*Author:      New Rupture Systems                                            */
/*Description: Function definitions for Basic Control Protocol.               */
/******************************************************************************/
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "BCP.h"

/*BCP version supported by this library*/
#define BCP_VERSION_SUPPORTED (0x10)

/*Host Requests*/
#define REQ_DEVICE_INFO  (0x00)
#define REQ_SET_FLAGS    (0x01)
#define REQ_SET_ADDRESS  (0x02)
#define REQ_READ_MEMORY  (0x03)
#define REQ_WRITE_MEMORY (0x04)

/*Device Responses*/
#define RSP_NONE    (0x00)
#define RSP_DATA    (0x01)
#define RSP_INVALID (0x02)

/*Magic numbers*/
#define PROPERTY_BCP_VERSION (0x00)
#define CRC_POLY             (0xC5)

/*Helper macros*/
#define BCP_SET_RR(bcp, rr)   (bcp)->pkt[0x00] &= 0x1F; \
                              (bcp)->pkt[0x00] |= ((rr) << 0x05);
#define BCP_SET_SIZE(bcp, sz) (bcp)->pkt[0x00] &= 0xF8; \
                              (bcp)->pkt[0x00] |= (sz);
#define BCP_GET_RR(bcp)       (unsigned char)(((bcp)->pkt[0x00] & 0xE0) >> 0x05)
#define BCP_GET_SIZE(bcp)     (unsigned char)((bcp)->pkt[0x00] & 0x07)
#define BCP_DATA(bcp)         (&((bcp)->pkt[0x01]))
#define NTOH64(x) swap64((x))
#define HTON64(x) swap64((x))

union size64
{
   unsigned char u8[sizeof(unsigned long long)];
   unsigned long long u64;
};

static bool send(struct BCP_Session *restrict);
static bool receive(struct BCP_Session *restrict);
static bool isEvenParity(unsigned char);
static unsigned char calculateCRC(const unsigned char *restrict, unsigned char);
static unsigned long long swap64(unsigned long long);


#if defined(BCP_HOST)
/* Initialize library for host use.
 *
 * INPUT : bcp - BCP session handle
 *         readDevice - callback to read data from device
 *         writeDevice - callback to write data to device
 * 
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool BCP_OpenHost(struct BCP_Session *restrict bcp,
                  bool (*readDevice)(void *, unsigned char),
                  bool (*writeDevice)(void *, unsigned char))
{
   bcp->read = readDevice;
   bcp->write = writeDevice;

   /*Get device BCP version*/
   BCP_SET_RR(bcp, REQ_DEVICE_INFO);
   BCP_SET_SIZE(bcp, 0x00);
   BCP_DATA(bcp)[0x00] = PROPERTY_BCP_VERSION;
   if(send(bcp))
   {
      bcp->error = 0x00;
      return true;
   }

   /*Check version is compatible with this library*/
   if((receive(bcp)) ||
      (BCP_GET_RR(bcp) != RSP_DATA) ||
      (BCP_GET_SIZE(bcp) != 0x00) ||
      (BCP_DATA(bcp)[0x00] > BCP_VERSION_SUPPORTED))
   {
      bcp->error = 0x01;
      return true;
   }

   return false;
}


/* Set device memory address (for future read/write requests).
 *
 * INPUT : bcp - BCP session handle
 *         address - device memory address to set
 * 
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool BCP_SetAddress(struct BCP_Session *restrict bcp,
                    unsigned long long address)
{
   /*Set address (and check that is was set)*/
   BCP_SET_RR(bcp, REQ_SET_ADDRESS);
   address = HTON64(address);
   memcpy(BCP_DATA(bcp), &address, 0x08);
   BCP_SET_SIZE(bcp, 0x07);

   if((send(bcp)) ||
      (receive(bcp)) ||
      (BCP_GET_RR(bcp) != RSP_NONE))
   {
      bcp->error = 0x02;
      return true;
   }

   return false;
}


/* Set device memory flags (for future read/write requests).
 *
 * INPUT : bcp - BCP session handle
 *         flags - device memory flags to set
 * 
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool BCP_SetFlags(struct BCP_Session *restrict bcp, const unsigned char flags)
{
   /*Set flags and check that they were set*/
   BCP_SET_RR(bcp, REQ_SET_FLAGS);
   BCP_SET_SIZE(bcp, 0x00);
   BCP_DATA(bcp)[0x00] = flags;

   if((send(bcp)) ||
      (receive(bcp)) ||
      (BCP_GET_RR(bcp) != RSP_NONE))
   {
      bcp->error = 0x02;
      return true;
   }

   return false;
}


/* Read device memory.
 *
 * INPUT : bcp - BCP session handle
 *         buffer - buffer to store read data
 *         size - size of buffer/data to be read
 * 
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool BCP_ReadMemory(struct BCP_Session *restrict bcp, void *restrict buffer,
                    unsigned char size)
{
   if((size == 0x00) ||
      (size > 0x08))
   {
      return true;
   }

   size -= 0x01;
   BCP_SET_RR(bcp, REQ_READ_MEMORY);
   BCP_SET_SIZE(bcp, 0x00);
   BCP_DATA(bcp)[0x00] = size;

   if((send(bcp)) ||
      (receive(bcp)) ||
      (BCP_GET_RR(bcp) != RSP_DATA) ||
      (BCP_GET_SIZE(bcp) != size))
   {
      bcp->error = 0x02;
      return true;
   }

   memcpy(buffer, BCP_DATA(bcp), size + 0x01);
   return false;
}


/* Write device memory.
 *
 * INPUT : bcp - BCP session handle
 *         buffer - buffer of data to write
 *         size - size of buffer/data to be written
 * 
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool BCP_WriteMemory(struct BCP_Session *restrict bcp,
                     const void *restrict buffer, unsigned char size)
{
   if((size == 0x00) ||
      (size > 0x08))
   {
      return true;
   }

   size -= 0x01;
   BCP_SET_RR(bcp, REQ_WRITE_MEMORY);
   BCP_SET_SIZE(bcp, size);
   memcpy(BCP_DATA(bcp), buffer, size + 0x01);

   if((send(bcp)) ||
      (receive(bcp)) ||
      (BCP_GET_RR(bcp) != RSP_NONE))
   {
      bcp->error = 0x02;
      return true;
   }

   return false;
}
#endif


#if defined(BCP_DEVICE)
/* Initialize library for device use.
 *
 * INPUT : bcp - BCP session handle
 *         readHost - callback for device to read data from host
 *         writeHost - callback for device to write to to host
 * 
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool BCP_OpenDevice(struct BCP_Session *restrict bcp,
                    bool (*readHost)(void *, unsigned char),
                    bool (*writeHost)(void *, unsigned char))
{
   bcp->flags = 0x00;
   bcp->address = 0x00ULL;
   bcp->read = readHost;
   bcp->write = writeHost;

   return false;
}


/* Handle incoming (host->device) requests.
 *
 * INPUT : bcp - BCP session handle
 *         reqRead - callout to handle incoming read requests
 *         reqWrite - callout to handle incoming write requests
 * 
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool BCP_HandleRequest(struct BCP_Session *restrict bcp,
                       bool (*reqRead)(unsigned long long, void *,
                       unsigned char),
                       bool (*reqWrite)(unsigned long long, void *,
                       unsigned char))
{
   /*Receive full request*/
   if(receive(bcp))
   {
      bcp->error = 0x02;
      return true;
   }

   /*Handle request*/
   switch(BCP_GET_RR(bcp))
   {
   case REQ_DEVICE_INFO:
      if(BCP_GET_SIZE(bcp) == 0x00)
      {
         switch(BCP_DATA(bcp)[0x00])
         {
         case PROPERTY_BCP_VERSION:
            BCP_SET_RR(bcp, RSP_DATA);
            BCP_DATA(bcp)[0x00] = BCP_VERSION_SUPPORTED;
            goto rspSet;
         }
      }
      break;
   case REQ_SET_FLAGS:
      if(BCP_GET_SIZE(bcp) == 0x00)
      {
         bcp->flags = 0x00;
         switch(BCP_DATA(bcp)[0x00])
         {
         case FLAG_ADDR_INC:
            bcp->flags |= FLAG_ADDR_INC;
            BCP_SET_RR(bcp, RSP_NONE);
            goto rspSet;
         }
      }
      break;
   case REQ_SET_ADDRESS:
      if(BCP_GET_SIZE(bcp) == 0x07)
      {
         memcpy(&bcp->address, BCP_DATA(bcp), 0x08);
         bcp->address = NTOH64(bcp->address);
         BCP_SET_RR(bcp, RSP_NONE);
         BCP_SET_SIZE(bcp, 0x00);
         goto rspSet;
      }
      break;
   case REQ_READ_MEMORY:
      if((BCP_GET_SIZE(bcp) == 0x00) &&
         (BCP_DATA(bcp)[0x00] < 0x08))
      {
         BCP_SET_SIZE(bcp, BCP_DATA(bcp)[0x00]);
         if(!reqRead(bcp->address, BCP_DATA(bcp), (BCP_DATA(bcp)[0x00]) + 0x01))
         {
            if(bcp->flags & FLAG_ADDR_INC)
            {
               bcp->address += (BCP_GET_SIZE(bcp) + 0x01);
            }
            
            BCP_SET_RR(bcp, RSP_DATA);
            goto rspSet;
         }
      }
      break;
   case REQ_WRITE_MEMORY:
      if(!reqWrite(bcp->address, BCP_DATA(bcp), BCP_GET_SIZE(bcp) + 0x01))
      {
         if(bcp->flags & FLAG_ADDR_INC)
         {
            bcp->address += (BCP_GET_SIZE(bcp) + 0x01);
         }
         
         BCP_SET_RR(bcp, RSP_NONE);
         BCP_SET_SIZE(bcp, 0x00);
         goto rspSet;
      }
      break;
   }

   BCP_SET_RR(bcp, RSP_INVALID);
   BCP_SET_SIZE(bcp, 0x00);
rspSet:
   if(send(bcp))
   {
      bcp->error = 0x02;
      return true;
   }

   return false;
}
#endif


/* Cleanup BCP session/library.
 *
 * INPUT : bcp - BCP session handle
 * 
 * OUTPUT: [None]
 */
void BCP_Close(struct BCP_Session *restrict bcp)
{
   /*Nothing to do*/
}


/* Retrieve error code (if an error occurred).
 *
 * INPUT : bcp - BCP session handle
 * 
 * OUTPUT: [None]
 */
unsigned int BCP_GetError(struct BCP_Session *restrict bcp)
{
   return bcp->error;
}


/* Retrieve error string (if an error occurred).
 *
 * INPUT : bcp - BCP session handle
 * 
 * OUTPUT: [None]
 */
const char *BCP_GetErrorString(struct BCP_Session *restrict bcp)
{
   const char *lookup[] =
   {
      "Unable to retrieve BCP version from device",
      "Device BCP version incompatible with this library",
      "General communication error"
   };

   return lookup[bcp->error];
}


/* Send request/response.
 *
 * INPUT : bcp - BCP session handle
 * 
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool send(struct BCP_Session *restrict bcp)
{
   unsigned char size = BCP_GET_SIZE(bcp);

   /*Set check bits*/
   bcp->pkt[0x00] &= 0xE7;
   if(!isEvenParity(BCP_GET_RR(bcp)))
   {
      bcp->pkt[0x00] |= 0x10;
   }

   if(!isEvenParity(size))
   {
      bcp->pkt[0x00] |= 0x08;
   }

   bcp->pkt[size + 0x02] = calculateCRC(bcp->pkt, size + 0x02);

   /*Send packet*/
   if(bcp->write(bcp->pkt, size + 0x03))
   {
      return true;
   }

   return false;
}


/* Receive request/response.
 *
 * INPUT : bcp - BCP session handle
 * 
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool receive(struct BCP_Session *restrict bcp)
{
   unsigned char size;

   /*Receive packet header*/
   if(bcp->read(bcp->pkt, 0x01))
   {
      return true;
   }
   
   /*Check if packet header is valid*/
   size = BCP_GET_SIZE(bcp);
   if((isEvenParity(BCP_GET_RR(bcp)) == ((bcp->pkt[0x00]) & 0x10)) ||
      (isEvenParity(size) == ((bcp->pkt[0x00]) & 0x08)))
   {
      return true;
   }

   /*Receive entire packet*/
   if(bcp->read(&(bcp->pkt[0x01]), size + 0x02))
   {
      return true;
   }

   /*Check if entire packet is valid*/
   if(bcp->pkt[size + 0x02] != calculateCRC(bcp->pkt, size + 0x02))
   {
      return true;
   }

   return false;
}


/* Determine if 3-bits have even parity.
 *
 * INPUT : bits - bits to analyze
 * 
 * OUTPUT: [Return] - true if bits have even parity, false otherwise
 */
bool isEvenParity(unsigned char bits)
{
   if((bits == 0x00) ||
      (bits == 0x03) ||
      (bits == 0x05) ||
      (bits == 0x06))
   {
      return true;
   }

   return false;
}


/* Calculate an 8-bit CRC for given data.
 *
 * INPUT : data - data to calculate CRC for
 *         size - size of data
 * 
 * OUTPUT: [Return] - CRC value
 */
unsigned char calculateCRC(const unsigned char *restrict data,
                           unsigned char size)
{
   bool crcMSB;
   unsigned char i;
   unsigned char src;
   bool last = false;
   unsigned char crc = 0xFF;

   while(!last)
   {
      /*Load next input byte (or last 0-extend byte)*/
      if(size == 0x00)
      {
         src = 0x00;
         last = true;
      }
      else
      {
         src = *data;
         data++;
         size--;
      }

      /*Add byte to bitstream*/
      for(i = 0x00; i < 0x08; i++)
      {
         crcMSB = (crc & 0x80);
         crc <<= 0x01;

         /*Add bit to bitstream*/
         if(src & 0x80)
         {
            crc |= 0x01;
         }
         src <<= 0x01;

         /*XOR if MSB(top-bit) is 1*/
         if(crcMSB)
         {
            crc ^= CRC_POLY;
         }
      }
   }
   
   return crc;
}


/* Swap incoming big-endian value to host endianess
 *
 * INPUT : u64 - value to (possibly) swap endianess from
 * 
 * OUTPUT: [Return] - Value with endianess (possibly) swapped
 */
unsigned long long swap64(unsigned long long u64)
{
   union size64 result;
   
   result.u8[0x00] = (unsigned char)((u64 & 0xFF00000000000000) >> 0x38);
   result.u8[0x01] = (unsigned char)((u64 & 0x00FF000000000000) >> 0x30);
   result.u8[0x02] = (unsigned char)((u64 & 0x0000FF0000000000) >> 0x28);
   result.u8[0x03] = (unsigned char)((u64 & 0x000000FF00000000) >> 0x20);
   result.u8[0x04] = (unsigned char)((u64 & 0x00000000FF000000) >> 0x18);
   result.u8[0x05] = (unsigned char)((u64 & 0x0000000000FF0000) >> 0x10);
   result.u8[0x06] = (unsigned char)((u64 & 0x000000000000FF00) >> 0x08);
   result.u8[0x07] = (unsigned char)(u64 & 0x00000000000000FF);

   return result.u64;
}
