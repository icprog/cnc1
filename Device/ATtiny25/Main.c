/******************************************************************************/
/*Filename:    Main.c                                                         */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: USB<->I2C converter, fully bidirectional. USB communication    */
/*             occurs over Enpoint-0. I2C communcation is slave (this device) */
/*             to master with a slave rising/falling edge interrupt being     */
/*             generated on received USB data.                                */
/******************************************************************************/
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "usbdrv.h"

/*RX/TX packet buffer size*/
#define RXTXBUFSZ (0x0A)

/*Host Requests*/
#define VENDOR_RQ_RESET (0x00)
#define VENDOR_RQ_READ  (0x01)
#define VENDOR_RQ_WRITE (0x02)

/*I2C slave address*/
#define I2C_ADDRESS (0x3A)

/*USI states*/
#define USI_STATE_NONE         (0x01)
#define USI_STATE_ADDRESS      (0x02)
#define USI_STATE_TX           (0x04)
#define USI_STATE_TX_ACK       (0x08)
#define USI_STATE_CHECK_TX_ACK (0x10)
#define USI_STATE_RX           (0x20)
#define USI_STATE_RX_ACK       (0x40)
#define USI_STATE_STOPPED      (0x80)

/*Data from USB host (Control-OUT endpoint)*/
uint8_t rxBuffer[RXTXBUFSZ];
uint8_t rxStart = 0x00;
uint8_t rxEnd = 0xFF;

/*Data to USB host (Control-IN endpoint)*/
uint8_t txBuffer[RXTXBUFSZ];
uint8_t txStart = 0x00;
uint8_t txEnd = 0xFF;


/* Process USB Vendor-defined requests.
 *
 * INPUT : data - request data (ID, size, etc.)
 *
 * OUTPUT: [return] - 0 if request unknown, (USB_NO_MSG) otherwise
 */
usbMsgLen_t usbFunctionSetup(uchar data[8])
{
   usbRequest_t *rq = (usbRequest_t *)data;
   uint8_t avail;

   if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_VENDOR)
   {
      switch(rq->bRequest)
      {
      case VENDOR_RQ_READ:
         /*Check if read data availible (may be less size then requested)*/
         if(txEnd == 0xFF)
         {
            return 0x00;
         }
         break;
      case VENDOR_RQ_WRITE:
         /*Check if write space availible (must be full size requested)*/
         if(rxEnd == 0xFF)
         {
            avail = RXTXBUFSZ;
         }
         else if(rxEnd > rxStart)
         {
            avail =  RXTXBUFSZ - (rxEnd - rxStart);
         }
         else
         {
            avail = (rxStart - rxEnd);
         }

         if(rq->wLength.bytes[0x00] > avail)
         {
            return 0x00;
         }
         break;
      case VENDOR_RQ_RESET:
         /*Reset buffers (recovery mechanism)*/
         rxStart = 0x00;
         rxEnd = 0xFF;
         txStart = 0x00;
         txEnd = 0xFF;
      default:
         return 0x00;
      }
   }

   return USB_NO_MSG;
}


/* Process USB RX from host (read) operation.
 *
 * INPUT : data - data received from host
 *         len - size of data buffer
 *
 * OUTPUT: [return] - 0 if more data expected, !0 otherwise
 */
uchar usbFunctionWrite(uchar *data, uchar len)
{
   uint8_t i;

   /*If buffer empty, fix ref*/
   if((rxEnd == 0xFF) && (len))
   {
      rxEnd = rxStart;
   }

   /*memcpy()*/
   for(i = 0x00; i < len; i++)
   {
      *(rxBuffer + rxEnd) = data[i];
      rxEnd++;
      rxEnd %= RXTXBUFSZ;

      if(rxStart == rxEnd)
      {
         break;
      }
   }

   return (i == len);
}


/* Process USB TX to host (write) operations.
 *
 * INPUT : data - data to send to host
 *         len - size of data requested
 *
 * OUTPUT: [return] - size of data sent (or to be sent)
 */
uchar usbFunctionRead(uchar *data, uchar len)
{
   uint8_t i;

   /*memcpy()*/
   for(i = 0x00; i < len; i++)
   {
      data[i] = *(txBuffer + txStart);
      txStart++;
      txStart %= RXTXBUFSZ;

      /*Check if buffer now empty*/
      if(txStart == txEnd)
      {
         i++;
         txEnd = 0xFF;
         break;
      }
   }

   return i;
}


int main(void)
{
   uint8_t usiState = 0x00;

   /*I2C - Initialize USI*/
   /*SDA(PB0)/SCL(PB2) HIGH*/
   PORTB |= 0x05;
   /*SDA input/SCL on output (ready to detect start condition)*/
   DDRB |= 0x04;
   /*Setup start detector/USI*/
   USICR = 0x28;
   /*Clear bits(set-to-clear)/reset counter*/
   USISR = 0xF0;

   wdt_enable(WDTO_1S);
   usbInit();
   usbDeviceDisconnect();

   /*Virtual reset of USB host-to-device*/
   while(--usiState)
   {
      wdt_reset();
      _delay_ms(0x01);
   }

   /*Setup and start timer*/
   TCNT1 = 0x00;
   OCR1A = 0xFF;
   TCCR1 = 0x0D;

   usiState = USI_STATE_NONE;
   usbDeviceConnect();
   sei();
   while(0x01)
   {
      wdt_reset();
      usbPoll();

      /*I2C - Check for START condition and SCL LOW*/
      if((USISR & 0x80) && (!(PINB & 0x04)))
      {
         /*SDA on input*/
         DDRB &= 0xFE;

         /*Expect address octet*/
         usiState = USI_STATE_ADDRESS;

         /*Set overflow logic to hold SCL on overflow*/
         USICR = 0x38;
         /*Clear START detector logic to release SCL and set counter*/
         USISR = 0xF0;
      }

      /*I2C - Check for overflow, SCL LOW*/
      if(USISR & 0x40)
      {
         switch(usiState)
         {
         case USI_STATE_ADDRESS:
            /*Check for address match or broadcast address*/
            if((!USIDR) || ((USIDR >> 0x01) == I2C_ADDRESS))
            {
               if(USIDR & 0x01)
               {
                  /*TX to master (read from RX buffer)*/
                  /*Check if data is availible to be read*/
                  if(rxEnd == 0xFF)
                  {
                     goto usiReset;
                  }
                  usiState = USI_STATE_TX;
               }
               else
               {
                  /*TX from master (write to TX buffer)*/
                  /*Check if buffer is full*/
                  if(txStart == txEnd)
                  {
                     goto usiReset;
                  }
                  usiState = USI_STATE_RX;
               }

               /*SDA on output*/
               DDRB |= 0x01;
               /*Addressed so ACK master*/
               USIDR = 0x00;
               USISR = 0x7E;
            }
            else
            {
usiReset:
               /*Not addressed, reset for new start*/
               usiState = USI_STATE_NONE;
               USICR = 0x28;
               USISR = 0x70;
            }
            break;
         case USI_STATE_CHECK_TX_ACK:
            if(USIDR)
            {
               /*Master NAK, TX no more data*/
               goto usiReset;
            }
            /*Else fall-through and keep TX'ing*/
         case USI_STATE_TX:
            /*SDA on output to send data*/
            DDRB |= 0x01;

            /*Transmit 0xA3 on over-read*/
            if(rxEnd == 0xFF)
            {
               USIDR = 0xA3;
            }
            else
            {
               /*TX buffer*/
               USIDR = *(rxBuffer + rxStart);
               rxStart++;
               rxStart %= RXTXBUFSZ;

               /*Check if buffer is now empty*/
               if(rxStart == rxEnd)
               {
                  rxEnd = 0xFF;
               }
            }

            usiState = USI_STATE_TX_ACK;
            USISR = 0x70;
            break;
         case USI_STATE_TX_ACK:
            /*SDA on input to read master ACK*/
            DDRB &= 0xFE;
            USIDR = 0x00;
            usiState = USI_STATE_CHECK_TX_ACK;
            USISR = 0x7E;
            break;
         case USI_STATE_RX:
            /*SDA on input to receive data*/
            DDRB &= 0xFE;
            usiState = USI_STATE_RX_ACK;
            USISR = 0x70;
            break;
         case USI_STATE_RX_ACK:
            /*If buffer empty, fix ref*/
            if(txEnd == 0xFF)
            {
               txEnd = txStart;
            }
            else if(txStart == txEnd)
            {
               /*This RX won't fit, NACK*/
               goto usiReset;
            }

            /*Write to buffer*/
            *(txBuffer + txEnd) = USIDR;
            txEnd++;
            txEnd %= RXTXBUFSZ;

            /*Data written, ACK*/
            /*SDA on output to send ACK*/
            DDRB |= 0x01;
            USIDR = 0x00;
            usiState = USI_STATE_RX;
            USISR = 0x7E;
            break;
         }
      }

      /*I2C - Check for STOP condition (SCL/SDA HIGH)*/
      if(USISR & 0x20)
      {
         usiState = USI_STATE_STOPPED;
      }

      /*I2C - Interrupt master(SCL rising-edge interrupt) if data ready*/
      if(rxEnd != 0xFF)
      {
         /*Check bus is idle*/
         if((usiState == USI_STATE_NONE) ||
            (usiState == USI_STATE_STOPPED))
         {
            /*Master should be interrupted by rising edge on SCL*/
            cli();
            if(TIFR & 0x40)
            {
               USICR |= 0x01;
               USICR &= 0xFE;
               USISR = 0xF0;
               TCNT1 = 0x00;
               TIFR |= 0x40;
            }
            sei();
         }
      }
   }

   return 0x00;
}
