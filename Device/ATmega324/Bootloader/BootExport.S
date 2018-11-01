/******************************************************************************/
/*Filename:    BootExport.S                                                   */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Declarations for definitions implemented in bootloader space.  */
/******************************************************************************/
.section .bootexport,"ax",@progbits

.extern TWI_ISR
.extern TWI_Poll
.extern TWI_StartWrite
.extern TWI_StartRead
.extern BCP_Init_wrapper
.extern BCP_HandleRequest

.global BootExport
BootExport:
   /*Each entry below should be a 4 byte jmp instruction*/
   jmp TWI_ISR
   jmp TWI_Poll
   jmp TWI_StartWrite
   jmp TWI_StartRead
   jmp BCP_Init_wrapper
   jmp BCP_HandleRequest
