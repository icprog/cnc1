/******************************************************************************/
/*Filename:    Main.c                                                         */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Bootloader for CNC primary controller device. When jumper/PB is*/
/*             GND device may receive (via I2C/BCP) request to write main     */
/*             application section. If jumper is left pulled-up main          */
/*             application is started immediately.                            */
/******************************************************************************/
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "BCP.h"
#include "TWI.h"

/*General constants*/
#define I2C_ADDRESS     (0x3A)
#define FLASH_PAGE_SIZE (0x80)
#define FLASH_PAGE_MASK (0xFF80)
#define FLASH_END       (0x8000)

/*General flags*/
#define FLAG_TWI_INT        (0x01)
#define FLAG_PRGRM_UNLOCKED (0x02)
#define FLAG_OUTSTANDING    (0x04)

/*Global variables*/
unsigned int writeAddress;
unsigned char writeCount;
unsigned char writeBuffer[FLASH_PAGE_SIZE];
unsigned char bootMsg[0x08] = {'B', 'O', 'O','T', 'L', 'O', 'A', 'D'};
volatile unsigned char flags = 0x00;


/* Read data from I2C bus (used for BCP).
 *
 * INPUT : data - output buffer for read data
 *         size - size of data to be read
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool devRead(void *data, unsigned char size)
{
   unsigned char remaining;

   /*Read data for I2C bus*/
   if(!TWI_StartRead(I2C_ADDRESS, data, size))
   {
      return true;
   }

   /*Wait for read operation to complete*/
   while(!TWI_Poll(&remaining));
   if(remaining)
   {
      return true;
   }

   return false;
}


/* Write data to I2C bus (used for BCP).
 *
 * INPUT : data - input buffer for write data
 *         size - size of data to be written
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool devWrite(void *data, unsigned char size)
{
   unsigned char remaining;

   /*Write data to I2C bus*/
   if(!TWI_StartWrite(I2C_ADDRESS, data, size))
   {
      return true;
   }

   /*Wait for write operation to complete*/
   while(!TWI_Poll(&remaining));
   if(remaining)
   {
      return true;
   }

   return false;
}


/* Read FLASH memory page into temp-buffer.
 *
 * INPUT : addr - address of flash page to load
 *
 * OUTPUT: [None]
 */
void readPage(unsigned int addr)
{
   unsigned char i;

   for(i = 0x00; i < FLASH_PAGE_SIZE; i++)
   {
      writeBuffer[i] = pgm_read_byte(addr + i);
   }
}


/* Write buffer page to FLASH memory.
 *
 * INPUT : addr - address of flash page to write
 *
 * OUTPUT: [None]
 */
void writePage(unsigned int addr)
{
   unsigned char i;
   uint16_t writeWord;

   if(writeCount != 0xFF)
   {
      writeCount++;
   }

   flags &= (~FLAG_OUTSTANDING);

   /*Erase page*/
   boot_page_erase(addr);
   boot_spm_busy_wait();

   /*Fill temp-buffer*/
   for(i = 0x00; i < FLASH_PAGE_SIZE; i += 0x02)
   {
      writeWord = writeBuffer[i];
      writeWord |= (((uint16_t)writeBuffer[i + 0x01]) << 0x08);
      boot_page_fill(addr + i, writeWord);
   }

   /*Write temp-buffer to FLASH*/
   boot_page_write(addr);
   boot_spm_busy_wait();

   /*Re-enable application FLASH memory for reading*/
   boot_rww_enable();
}


/* Process memory read commands.
 *
 * INPUT : addr - address for read
 *         data - output buffer for read data
 *         size - size of data to be read
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool memRead(unsigned long long addr, void *data, unsigned char size)
{
   unsigned char offset;
   unsigned char *buf = data;

   /*Last 8 bytes make up 8 byte string ID*/
   if(addr >= 0xFFFFFFFFFFFFFFF8ULL)
   {
      offset = (addr & 0x07);
      if(size > (0x08 - offset))
      {
         return true;
      }

      memcpy(data, bootMsg + offset, size);
      return false;
   }
   /*Return pages written since last commit*/
   else if((addr == 0xFFFFFFFFFFFFFFF7ULL) &&
           (size == 0x01))
   {
      buf[0x00] = writeCount;
      return false;
   }

   /*FLASH memory is mapped into bottom of address space*/
   if(addr < (unsigned long long)FLASH_END)
   {
      for(offset = 0x00; offset < size; offset++)
      {
         buf[offset] = pgm_read_byte((uint16_t)addr + offset);
      }

      return false;
   }

   return true;
}


/* Process memory write commands.
 *
 * INPUT : addr - address for write
 *         data - input buffer for write data
 *         size - size of data to be written
 *
 * OUTPUT: [Return] - true if an error occurred, false otherwise
 */
bool memWrite(unsigned long long addr, void *data, unsigned char size)
{
   unsigned char i;
   unsigned char *buf = data;
   bool ret = true;

   cli();

   /*Lock/Unlock/Commit application memory*/
   if((addr == 0x010000ACE0000010ULL) &&
      (size == 0x01))
   {
      if(buf[0x00] == 0x00)
      {
         /*If data is still in temp-write buffer, flush to FLASH*/
         if(flags & FLAG_OUTSTANDING)
         {
            writePage(writeAddress - (writeAddress % FLASH_PAGE_SIZE));
         }

         flags &= (~FLAG_PRGRM_UNLOCKED);
      }
      else if(buf[0x00] == 0x01)
      {
         flags |= FLAG_PRGRM_UNLOCKED;
         flags &= (~FLAG_OUTSTANDING);
         writeCount = 0x00;
         writeAddress = 0x00;
         readPage(writeAddress);
      }
      else
      {
         goto error;
      }

      goto done;
   }

   /*Bounds/state check*/
   if((addr >= (unsigned long long)FLASH_END) ||
      ((addr + size) >= (unsigned long long)FLASH_END) ||
      (!(flags & FLAG_PRGRM_UNLOCKED)))
   {
      goto error;
   }

   /*Flush write-buffer if new address is outside page of last address*/
   if(((unsigned int)addr & FLASH_PAGE_MASK) !=
      (writeAddress & FLASH_PAGE_MASK))
   {
      if(flags & FLAG_OUTSTANDING)
      {
         writePage(writeAddress - (writeAddress % FLASH_PAGE_SIZE));
         readPage((unsigned int)addr - ((unsigned int)addr % FLASH_PAGE_SIZE));
      }
   }
   writeAddress = (unsigned int)addr;

   /*Modify page in buffer*/
   for(i = 0x00; i < size; i++)
   {
      flags |= FLAG_OUTSTANDING;
      writeBuffer[writeAddress % FLASH_PAGE_SIZE] = buf[i];
      writeAddress++;

      if(!(writeAddress % FLASH_PAGE_SIZE))
      {
         writePage(writeAddress - FLASH_PAGE_SIZE);
         readPage(writeAddress);
      }
   }

done:
   ret = false;
error:
   sei();
   return ret;
}


/*Interrupt vector for USB<->I2C slave*/
ISR(PCINT2_vect)
{
   if(PCMSK2 & 0x01)
   {
      flags |= FLAG_TWI_INT;
   }
}


/*Interrupt vector for I2C module*/
ISR(TWI_vect)
{
   TWI_ISR();
}


/* Open new BCP session for application (EXPORT).
 *
 * INPUT : bcp - bcp session
 *
 * OUTPUT: [None]
 */
void BCP_Init_wrapper(struct BCP_Session *bcp)
{
   BCP_OpenDevice(bcp, devRead, devWrite);
}


int main(void)
{
   struct BCP_Session bcp;

   /*Lock bootloader from SPM writes*/
   if(boot_lock_fuse_bits_get(GET_LOCK_BITS) & (!(_BV(BLB11))))
   {
      boot_lock_bits_set (_BV(BLB11));
   }

   /*Enable internal pull-up on bootloader-hold pin*/
   PORTB = 0x01;

   /*Startup delay (external peripherals voltage build time)*/
   _delay_ms(100);

   /*Initialze libraries*/
   TWI_Initialize();
   BCP_OpenDevice(&bcp, devRead, devWrite);

   if(PINB & 0x01)
   {
      /*PB on pin not GND'd, jump to application*/
      asm("jmp 0x00");
   }

   /*Set other pins*/
   PCICR |= 0x04;
   PCMSK2 |= 0x01;

   /*Move IV table to bootloader section*/
   MCUCR = 0x01;
   MCUCR = 0x02;

   sei();
   for(;;)
   {
      if(flags & FLAG_TWI_INT)
      {
         BCP_HandleRequest(&bcp, memRead, memWrite);
         flags &= (~(FLAG_TWI_INT));
      }
   }

   return 0x00;
}
