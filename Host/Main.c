/******************************************************************************/
/*Filename:    Main.c                                                         */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: BCP host program to send commands to device.                   */
/******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include "libusb.h"
#include "Platform.h"
#include "BCP.h"
#include "Flash.h"

static void outputUsage(void);
static void flashProgress(void);
static void shutdownHook(int);
static bool rwDevice(unsigned char *, unsigned char, bool);
static bool hostRead(void *, unsigned char);
static bool hostWrite(void *, unsigned char);
static void cbTransfer(struct libusb_transfer *);

/*Global variables*/
static volatile sig_atomic_t exitSignal;
static libusb_device_handle *deviceHandle = NULL;


int main(int argc, char *argv[])
{
   libusb_device **deviceList;
   ssize_t cnt;
   struct libusb_device_descriptor deviceInfo;
   struct BCP_Session bcp;
   struct Flash_Session flash;
   unsigned char pages;
   unsigned int bytes;
   int err;
   int ret = EXIT_FAILURE;

   /*Check [option] is provided*/
   if(argc < 0x02)
   {
      outputUsage();
      return ret;
   }

   /*Establish SIGINT handler (for a cleaner SIGINT shutdown)*/
   exitSignal = false;
   signal(SIGINT, shutdownHook);

   /*Establish USB link with device*/
   err = libusb_init(NULL);
   if(err)
   {
      printf("Error: Failed to initialize USB library (%d)\n", err);
      return ret;
   }

   cnt = libusb_get_device_list(NULL, &deviceList);
   if(cnt < 0x00)
   {
      printf("Error: Failed to enumerate USB devices (%d)\n", (int)cnt);
      goto usbDeinit;
   }

   while(cnt--)
   {
      if(libusb_get_device_descriptor(deviceList[cnt], &deviceInfo))
      {
         continue;
      }

      if((deviceInfo.bcdUSB == 0x0110) &&
         (deviceInfo.bDeviceClass == 0xFF) &&
         (deviceInfo.bDeviceSubClass == 0x00) &&
         (deviceInfo.idVendor == 0xF055) &&
         (deviceInfo.idProduct == 0x3A3A))
      {
         err = libusb_open(deviceList[cnt], &deviceHandle);
         if(err)
         {
            printf("Error: Failed to open device for transfers (%d)\n", err);
            goto usbDeinit;
         }
         break;
      }
   }
   libusb_free_device_list(deviceList, 0x01);

   if(deviceHandle == NULL)
   {
      printf("Error: Device not found\n");
      goto usbDeinit;
   }

   /*Establish BCP link (over USB transport)*/
   if(BCP_OpenHost(&bcp, hostRead, hostWrite))
   {
      printf("Error: Failed to open BCP interface to device\n" \
             "Reason: %s\n", BCP_GetErrorString(&bcp));
      goto usbClose;
   }

   /*Attempt to execute option specified*/
   if(strcmp(argv[0x01], "flash") == 0x00)
   {
      if(argc != 0x03)
      {
         printf("Error: option 'flash' expected <filename>\n");
         goto bcpClose;
      }

      printf("--Flashing Device--\n");
      if(Flash_Open(&flash, &bcp, argv[0x02]))
      {
         printf("Error: %s\n", Flash_GetErrorString(&flash));
         goto bcpClose;
      }

      if((printf("Writing:\n["), Flash_Write(&flash, flashProgress, 0x02)) ||
         (printf("]\nVerifying:\n["), Flash_Verify(&flash, flashProgress, 0x02)))
      {
         printf("]\nError: %s\n", Flash_GetErrorString(&flash));
         Flash_Close(&flash);
         goto bcpClose;
      }
      else
      {
         if(Flash_GetSize(&flash, &pages, &bytes))
         {
            printf("]\nDevice sucessfully flashed (#, #)\n");
         }
         else
         {
            printf("]\nDevice successfully flashed (%u pages, %u bytes)\n",
                   (unsigned int)pages, bytes);
         }
         Flash_Close(&flash);
      }
   }
   else
   {
      printf("Error: Unknown option specified\n");
      outputUsage();
      goto bcpClose;
   }

   ret = EXIT_SUCCESS;
bcpClose:
   BCP_Close(&bcp);
usbClose:
   libusb_close(deviceHandle);
usbDeinit:
   libusb_exit(NULL);
   return ret;
}


/* Handle SIGINT requests (libusb functions should return early).
 *
 * INPUT : sig - signal that fired
 *
 * OUTPUT: [None]
 */
void shutdownHook(int sig)
{
   exitSignal = true;

   /*Re-arm (to keep control in program)*/
   signal(SIGINT, shutdownHook);
}


/* Output program usage message.
 *
 * INPUT : [None]
 *
 * OUTPUT: [None]
 */
void outputUsage(void)
{
   printf("Usage: cncControl [option] ...\n");
   printf("Options:\n");
   printf("   flash <filename> - Write provided Intel Hex file to device\n");
}


/* Output progress of flash write/verify operation.
 *
 * INPUT : [None]
 *
 * OUTPUT: [None]
 */
void flashProgress(void)
{
   putchar('#');
   fflush(stdout);
}


/* Passthrough BCP read requests to USB.
 *
 * INPUT : size - size of input buffer
 *
 * OUTPUT: data - buffer for read data
 *         [Return] - true if an error occurred, false otherwise
 */
bool hostRead(void *data, unsigned char size)
{
   bool ret;

   ret = rwDevice(data, size, true);
   return ret;
}


/* Passthrough BCP write requests to USB.
 *
 * INPUT : data - buffer of data to be written
 *         size - size of input buffer
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool hostWrite(void *data, unsigned char size)
{
   bool ret;

   ret = rwDevice(data, size, false);
   return ret;
}


/* Main function to read/write USB data.
 *
 * INPUT : data - buffer read/write data
 *         size - size of read/write buffer
 *         read - true if read, false if write
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool rwDevice(unsigned char *data, unsigned char size, bool read)
{
   struct libusb_transfer *transfer;
   unsigned char _buffer[LIBUSB_CONTROL_SETUP_SIZE + size + 0x01];
   unsigned char rwSize;
   unsigned char *buffer = _buffer;
   unsigned char offset = 0x00;
   unsigned char attempt = 0x05;
   bool ret = true;

   if((size == 0x00 )||
      (exitSignal))
   {
      return true;
   }

   /*libusb needs 16-bit aligned buffer*/
   if(((uintptr_t)buffer) & 0x01)
   {
      buffer++;
   }

   /*Allocate a transfer structure*/
   transfer = libusb_alloc_transfer(0x00);
   if(transfer == NULL)
   {
      return true;
   }

   while(attempt--)
   {
      /*Fill transfer (USB 1s timeout)*/
      if(read)
      {
         libusb_fill_control_setup(buffer, 0xC0, 0x01, 0x00, 0x00, size);
      }
      else
      {
         libusb_fill_control_setup(buffer, 0x40, 0x02, 0x00, 0x00, size);
         memcpy(buffer + LIBUSB_CONTROL_SETUP_SIZE, data + offset, size);
      }
      libusb_fill_control_transfer(transfer, deviceHandle, buffer, cbTransfer,
                                   &rwSize, 0x03E8);

      if(libusb_submit_transfer(transfer) != 0x00)
      {
         goto error;
      }

      /*Wait(block) for transfer completion*/
      if(libusb_handle_events_completed(NULL, NULL) != 0x00)
      {
         goto error;
      }

      if(read)
      {
         memcpy(data + offset, buffer + LIBUSB_CONTROL_SETUP_SIZE, rwSize);
      }

      /*Check if transfer completed fully*/
      if(rwSize == size)
      {
         /*Artificially delay write's (to give time for device roundtrip)*/
         if(!read)
         {
            /*200ms delay*/
            Platform_SleepMS(0xC8);
         }

         ret = false;
         break;
      }
      else
      {
         size -= rwSize;
         offset += rwSize;
      }

      Platform_Sleep(0x01);
   }

   goto done;
error:
   libusb_cancel_transfer(transfer);
done:
   libusb_free_transfer(transfer);
   return ret;
}


/* Callback function for USB read/write transfers.
 *
 * INPUT : transfer - Transfer that completed
 */
void cbTransfer(struct libusb_transfer *transfer)
{
   *((unsigned char *)transfer->user_data) =
                                         (unsigned char)transfer->actual_length;
}
