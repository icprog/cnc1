/******************************************************************************/
/*Filename:    TWI.S                                                          */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Implementation/definitions for I2C/TWI module.                 */
/******************************************************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
#include "TWI.h"

;TWI_Flags bits[1:0]:
; 0x00 - No transfer in progress
; 0x01 - Transfer in progress
; 0x02 - Transfer succeeded
; 0x03 - Transfer failed

;The below option enables/disables the data ready interrupt pin
#define READ_INT 0x01


.global TWI_ISR
TWI_ISR:                               ;=======INTERRUPT_SERVICE_ROUTINE========
   push R20
   push R21
   push XL
   push XH
   
   lds R20, TWSR                       ;Load state
   andi R20, 0xF8
   mov XL, TWI_AddressL
   mov XH, TWI_AddressH
   mov R21, TWI_Size

   cpi R20, 0x18                       ;TX status codes
   breq TWI_ISR_txDataACK
   cpi R20, 0x20
   breq TWI_ISR_addressNAK
   cpi R20, 0x28
   breq TWI_ISR_txDataACK
   cpi R20, 0x30
   breq TWI_ISR_txDataNAK
   cpi R20, 0x40                       ;RX status codes
   breq TWI_ISR_rxAddressACK
   cpi R20, 0x48
   breq TWI_ISR_addressNAK
   cpi R20, 0x50
   breq TWI_ISR_rxDataACK
   cpi R20, 0x58
   breq TWI_ISR_rxDataACK

   ldi R20, 0x84                       ;Default: Lines to idle (INT-misfire)
   sts TWCR, R20
   rjmp TWI_ISR_done
TWI_ISR_txDataNAK:
   ldi R20, 0x01
   add R21, R20
   ;fallthrough to mark failure
TWI_ISR_addressNAK:
   rjmp TWI_ISR_failure
TWI_ISR_txDataACK:
   cpi R21, 0x00
   breq TWI_ISR_success
   dec R21
   ld R20, X+
   sts TWDR, R20
   ldi R20, 0x85
   sts TWCR, R20
   rjmp TWI_ISR_done
TWI_ISR_rxDataACK:
   lds R20, TWDR
   st X+, R20
   dec R21
   ;fallthrough to calculate next ACK/NAK
TWI_ISR_rxAddressACK:
   cpi R21, 0x00
   breq TWI_ISR_success
   ldi R20, 0x85
   cpi R21, 0x01
   breq TWI_ISR_rxAddressACK_nack
   ori R20, 0x40
TWI_ISR_rxAddressACK_nack:
   sts TWCR, R20
   rjmp TWI_ISR_done

TWI_ISR_success:
   mov R20, TWI_Flags                  ;Set transfer success flag
   andi R20, (~(0x01 << TWI_FLAG_BIT0))
   ori R20, (0x02 << TWI_FLAG_BIT0)
   rjmp TWI_ISR_complete
TWI_ISR_failure:
   mov R20, TWI_Flags                  ;Set transfer failure flag
   ori R20, (0x01 << TWI_FLAG_BIT0)
TWI_ISR_complete:
   mov TWI_Flags, R20
   ldi R20, 0x94                       ;Send STOP condition
   sts TWCR, R20
#ifdef READ_INT
   lds R20, PCMSK2
   ori R20, 0x01
   sts PCMSK2, R20
#endif
TWI_ISR_done:
   mov TWI_AddressL, XL
   mov TWI_AddressH, XH
   mov TWI_Size, R21
   pop XH
   pop XL
   pop R21
   pop R20
   reti


.global TWI_Initialize
TWI_Initialize:                        ;=============INITIALIZATION=============
   ldi R18, 0x02                       ;Initialize TWI clock(SCL) frequency
   sts _SFR_IO_ADDR(TWBR), R18         ;Set divider and pre-scaler (375 Khz)
   mov R18, TWI_Flags                  ;Clear transfer-in-progress flag
   andi R18, ~(0x03 << TWI_FLAG_BIT0)
   mov TWI_Flags, R18
   ret


.global TWI_Poll
TWI_Poll:                              ;****************FUNCTION****************
   movw R30, R24                       ;Check if transfer complete (fill size
   clr R24                             ;remaining to be transfered)
   mov R18, TWI_Flags                  ;==================================
   andi R18, (0x03 << TWI_FLAG_BIT0)   ;INPUT : Location to place remainder
   sbrs R18, ((0x02 << TWI_FLAG_BIT0) - 0x01);OUTPUT: None
   ret                                 ;****************************************
   mov R24, TWI_Size
   st Z, R24
   ldi R24, 0x01
   ret


.global TWI_StartWrite
TWI_StartWrite:                        ;****************FUNCTION****************
   sbrs R24, 0x07                      ;Transmit (0-255) octets of data using
   rjmp TWI_StartWrite_transfer        ;the TWI transport.
   clr R24                             ;========================================
   ret                                 ;INPUT : slave address, buffer,
TWI_StartWrite_transfer:               ;        buffer size
   lsl R24                             ;OUTPUT: !0 on success, 0 failure
   rcall TWI_startTransfer             ;****************************************
   ret


.global TWI_StartRead
TWI_StartRead:                         ;****************FUNCTION****************
   sbrs R24, 0x07                      ;Receive (0-255) octets of data using the
   rjmp TWI_StartRead_transfer         ;TWI transport.
   clr R24                             ;========================================
   ret                                 ;INPUT : slave address, buffer,
TWI_StartRead_transfer:                ;        buffer len
   lsl R24                             ;OUTPUT: !0 on success, 0 failure
   ori R24, 0x01                       ;****************************************
   rcall TWI_startTransfer
   ret


TWI_startTransfer:                     ;************INTERNAL_FUNCTION***********
   in R18, _SFR_IO_ADDR(SREG)          ;Setup and start transfer (direct call
   cli                                 ;from above 2 functions).
   mov R19, TWI_Flags                  ;****************************************
   andi R19, (0x03 << TWI_FLAG_BIT0)
   cpi R19, (0x01 << TWI_FLAG_BIT0)
   breq TWI_startTransfer_error

   ldi R19, 0xA4                       ;Send START condition
   sts TWCR, R19
TWI_startTransfer_poll:                ;Wait for START to be sent
   lds R19, TWCR
   sbrs R19, 0x07
   rjmp TWI_startTransfer_poll

   lds R19, TWSR                       ;Check START sent successfully
   andi R19, 0xF8
   cpi R19, 0x08
   brne TWI_startTransfer_error

   sts TWDR, R24                       ;Send SLA_RW
   ldi R19, 0x85
   sts TWCR, R19

   mov TWI_AddressL, R22
   mov TWI_AddressH, R23
   mov TWI_Size, R20
   mov R19, TWI_Flags
   andi R19, (~(0x03 << TWI_FLAG_BIT0))
   ori R19, (0x01 << TWI_FLAG_BIT0)    ;Set transfer-in-progress flag
   mov TWI_Flags, R19
   ldi R24, 0x01
   rjmp TWI_startTransfer_done
TWI_startTransfer_error:
   clr R24
TWI_startTransfer_done:
#ifdef READ_INT
   lds R19, PCMSK2
   andi R19, 0xFE
   sts PCMSK2, R19
#endif
   out _SFR_IO_ADDR(SREG), R18
   ret
