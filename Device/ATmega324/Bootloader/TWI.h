/******************************************************************************/
/*Filename:    TWI.h                                                          */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: I2C/TWI module implementation declarations.                    */
/******************************************************************************/
#ifndef TWI_H
#define TWI_H

#define TWI_FLAG_BIT0 0x00

#ifdef __ASSEMBLER__
#define TWI_Flags    R2
#define TWI_AddressL R3
#define TWI_AddressH R4
#define TWI_Size     R5
#else
#include <stdbool.h>
#include <stdint.h>

/*AVR has enough registers so permanetly reserve some*/
register uint8_t TWI_Flags    asm("r2");
register uint8_t TWI_AddressL asm("r3");
register uint8_t TWI_AddressH asm("r4");
register uint8_t TWI_Size     asm("r5");

void TWI_ISR(void);
bool TWI_Initialize(void);
bool TWI_Poll(unsigned char *);
bool TWI_StartWrite(unsigned char, unsigned char *, unsigned char);
bool TWI_StartRead(unsigned char, unsigned char *, unsigned char);

#endif
#endif
