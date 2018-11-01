/******************************************************************************/
/*Filename:    BootExport.c                                                   */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Definitions for functions exported by the device bootloader.   */
/******************************************************************************/
#include <stdbool.h>
#include "BootExport.h"


/* Interrupt service routine for TWI module.
 *
 * INPUT : [None]
 *
 * OUTPUT: [None]
 */
void TWI_ISR(void)
{
   ((void (*)(void))0x3FF4)();
}


/* Poll TWI transfer completion.
 *
 * INPUT : [None]
 *
 * OUTPUT: remainder - bytes not transferred (on failure)
 *         [Return] - true if transfer complete, false if still in progress
 */
bool TWI_Poll(unsigned char *remainder)
{
   return ((bool (*)(unsigned char *))0x3FF6)(remainder);
}


/* Start TWI write transfer.
 *
 * INPUT : addr - address to transfer to
 *         buf - buffer to transfer
 *         size - size of data to transfer
 *
 * OUTPUT: [Return] - true if write started, false otherwise
 */
bool TWI_StartWrite(unsigned char addr, unsigned char *buf, unsigned char size)
{
   return ((bool (*)(unsigned char, unsigned char *, unsigned char))0x3FF8)
           (addr, buf, size);
}


/* Start TWI read transfer.
 *
 * INPUT : addr - address to transfer from
 *         size - size of data to transfer
 *
 * OUTPUT: buf - buffer to store transfer
 *         [Return] - true if read started, false otherwise
 */
bool TWI_StartRead(unsigned char addr, unsigned char *buf, unsigned char size)
{
   return ((bool (*)(unsigned char, unsigned char *, unsigned char))0x3FFA)
           (addr, buf, size);
}


/* Open BCP session.
 *
 * INPUT : bcp - BCP session to open
 *
 * OUTPUT: [None]
 */
void BCP_Open(struct BCP_Session *bcp)
{
   ((void (*)(struct BCP_Session *))0x3FFC)(bcp);
}


/* Handle BCP device request.
 *
 * INPUT : bcp - BCP session
 *         rd - callback for read requests
 *         wr - callback for write requests
 *
 * OUTPUT: [Return] - false if request handled, true otherwise
 */
bool BCP_HandleRequest(struct BCP_Session *restrict bcp,
                       bool (*rd)(unsigned long long, void *, unsigned char),
                       bool (*wr)(unsigned long long, void *, unsigned char))
{
   return ((bool (*)(struct BCP_Session *const,
                     bool (*const)(unsigned long long, void *, unsigned char),
                     bool (*const)(unsigned long long, void *,
                                   unsigned char)))0x3FFE)(bcp, rd, wr);
}
