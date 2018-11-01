/******************************************************************************/
/*Filename:    BCP.h                                                          */
/*Project:     Basic Control Protocol                                         */
/*Author:      New Rupture Systems                                            */
/*Description: Middleware protocol to communicate with master-slave devices.  */
/******************************************************************************/
#ifndef BCP_H
#define BCP_H
#include <stdbool.h>

/* BCP Transmission Format (Version 1.0)
 *
 * Fields:
 * {REQ|RSP}(3-bit) | CHK(2-bit) | SIZE(3-bit) | DATA(1-8 bytes) | CRC(8-bit)
 *
 * REQ (Requests):
 * 0x00: REQ_DEVICE_INFO
 *     -> Property to retrieve (8-bit)
 *        - 0x00: Device supported BCP version
 *     <- Major.Minor version of BCP (4-bit each)
 * 0x01: REQ_SET_FLAGS
 *     -> Flags to set (8-bit)
 *        - 0x01: Auto-increment address
 *     <- [No Data Response]
 * 0x02: REQ_SET_ADDRESS
 *     -> Address (64-bit)
 *     <- [No Data Response]
 * 0x03: REQ_READ_MEMORY
 *     -> Read size (8-bit)
 *        - 0x00-0x07: Size to read
 *     <- Read Memory
 * 0x04: REQ_WRITE_MEMORY
 *     -> Write Memory
 *     <- [No Data Response]
 * 0x05: RESERVED
 * 0x06: RESERVED
 * 0x07: RESERVED
 * 
 * RSP (Responses):
 * 0x00: RSP_NONE
 *     - Request completed successfully, response contains no valid data
 * 0x01: RSP_DATA
 *     - Request completed successfully, response contains valid data
 * 0x02: RSP_INVALID
 *     - Invalid input was received, response contains no valid data
 *
 * CHK = Even parity (1-bit for first and last 3-bits)
 * SIZE = Data field size + 1
 * DATA = REQ/RSP Data
 * CRC = Cyclic redundancy check (polynomial 0xC5)
 */
#if !defined(BCP_HOST) && !defined(BCP_DEVICE)
#define BCP_HOST   0x01
#define BCP_DEVICE 0x01
#endif

#define FLAG_ADDR_INC (0x01)

struct BCP_Session
{
   unsigned char pkt[0x0A];
   unsigned char flags;
   unsigned long long address;
   bool (*read)(void *, unsigned char);
   bool (*write)(void *, unsigned char);
   unsigned int error;
};


#if defined(BCP_HOST)
/*Host interface*/
bool BCP_OpenHost(struct BCP_Session *restrict,
                  bool (*)(void *restrict, unsigned char),
                  bool (*)(void *restrict, unsigned char));
bool BCP_SetAddress(struct BCP_Session *restrict, unsigned long long);
bool BCP_SetFlags(struct BCP_Session *restrict, unsigned char);
bool BCP_ReadMemory(struct BCP_Session *restrict, void *restrict,
                    unsigned char);
bool BCP_WriteMemory(struct BCP_Session *restrict, const void *restrict,
                     unsigned char);
#endif

#if defined(BCP_DEVICE)
/*Device interface*/
bool BCP_OpenDevice(struct BCP_Session *restrict,
                    bool (*)(void *, unsigned char),
                    bool (*)(void *, unsigned char));
bool BCP_HandleRequest(struct BCP_Session *restrict,
                       bool (*)(unsigned long long, void *, unsigned char),
                       bool (*)(unsigned long long, void *, unsigned char));
#endif

/*Common interface*/
void BCP_Close(struct BCP_Session *restrict);
unsigned int BCP_GetError(struct BCP_Session *restrict);
const char *BCP_GetErrorString(struct BCP_Session *restrict);
#endif
