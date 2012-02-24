// Firmware framework for USB I/O on PIC 18F2455 (and siblings)
// Copyright (C) 2005 Alexander Enzmann
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
#ifndef USB_H
#define USB_H

#include <stdint.h>

#define PTR16(x) ((uint16_t)(((uint32_t)x) & 0xFFFFUL))

//
// Standard Request Codes USB 2.0 Spec Ref Table 9-4
//
#define GET_STATUS         0
#define CLEAR_FEATURE      1
#define SET_FEATURE        3
#define SET_ADDRESS        5
#define GET_DESCRIPTOR     6
#define SET_DESCRIPTOR     7
#define GET_CONFIGURATION  8
#define SET_CONFIGURATION  9
#define GET_INTERFACE     10
#define SET_INTERFACE     11
#define SYNCH_FRAME       12

// Descriptor Types
#define DEVICE_DESCRIPTOR        0x01
#define CONFIGURATION_DESCRIPTOR 0x02
#define STRING_DESCRIPTOR        0x03
#define INTERFACE_DESCRIPTOR     0x04
#define ENDPOINT_DESCRIPTOR      0x05

// Definitions from "Device Class Definition for Human Interface Devices (HID)",
// version 1.11
//

// Standard Feature Selectors
#define DEVICE_REMOTE_WAKEUP    0x01
#define ENDPOINT_HALT           0x00


// Buffer Descriptor bit masks (from PIC datasheet)
#define UOWN   0x80 // USB Own Bit
#define DTS    0x40 // Data Toggle Synchronization Bit
		    // Bits 4 + 5 are unimplemented (PIC18F25J50)
#define DTSEN  0x08 // Data Toggle Synchronization Enable Bit
#define BSTALL 0x04 // Buffer Stall Enable Bit
#define BC9    0x02 // Byte count bit 9
#define BC8    0x01 // Byte count bit 8

// Device states (Chap 9.1.1)
#define DETACHED     0
#define ATTACHED     1
#define POWERED      2
#define DEFAULT      3
#define ADDRESS      4
#define CONFIGURED   5

//
// Global variables
extern uint8_t deviceState;    // Visible device states (from USB 2.0, chap 9.1.1)
// extern uint8_t selfPowered;
extern uint8_t remoteWakeup;
extern uint8_t currentConfiguration;

// Every device request starts with an 8 uint8_t setup packet (USB 2.0, chap 9.3)
// with a standard layout.  The meaning of wValue and wIndex will
// vary depending on the request type and specific request.
typedef struct _setupPacketStruct
{
    uint8_t bmRequestType; // D7: Direction, D6..5: Type, D4..0: Recipient
    uint8_t bRequest;      // Specific request
    uint8_t wValue0;       // LSB of wValue
    uint8_t wValue1;       // MSB of wValue
    uint8_t wIndex0;       // LSB of wIndex
    uint8_t wIndex1;       // MSB of wIndex
    uint16_t wLength;       // Number of bytes to transfer if there's a data stage
    uint8_t extra[56];     // Fill out to same size as Endpoint 0 max buffer
} setupPacketStruct;

// USB Functions
void EnableUSBModule(void);

// Functions for reading/writing the HID interrupt endpoint
uint8_t InPipe(uint8_t pipe, volatile uint8_t *buffer, uint8_t len);
uint8_t OutPipe(uint8_t pipe, volatile uint8_t *buffer, uint8_t len);

#endif //USB_H
