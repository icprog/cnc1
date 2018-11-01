/******************************************************************************/
/*Filename:    BootExport.h                                                   */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Declare exported functions from bootloader.                    */
/******************************************************************************/
#ifndef BOOT_EXPORT_H
#define BOOT_EXPORT_H
#include <stdbool.h>
#include "Platform.h"

/*Borrowed from BCP.h*/
struct BCP_Session
{
   unsigned char pkt[0x0A];
   unsigned char flags;
   unsigned long long address;
   bool (*read)(void *, unsigned char);
   bool (*write)(void *, unsigned char);
};

/*Borrowed from TWI.h*/
register uint8_t TWI_Flags    asm("r2");
register uint8_t TWI_AddressL asm("r3");
register uint8_t TWI_AddressH asm("r4");
register uint8_t TWI_Size     asm("r5");


void TWI_ISR(void);
bool TWI_Poll(unsigned char *);
bool TWI_StartWrite(unsigned char, unsigned char *, unsigned char);
bool TWI_StartRead(unsigned char, unsigned char *, unsigned char);
void BCP_Open(struct BCP_Session *);
bool BCP_HandleRequest(struct BCP_Session *restrict,
                       bool (*)(unsigned long long, void *, unsigned char),
                       bool (*)(unsigned long long, void *, unsigned char));


/* Enable TWI interrupt.
 *
 * INPUT : enable - true to enable interrupt, false to disable
 *         cb - callback when interrupt is activated
 *
 * OUTPUT: [None]
 */
static inline void BootExport_EnableTWIINT2(bool enable, void (*cb)(void))
{
   TWI_ENABLE_INT2(enable, cb);
}

#endif
