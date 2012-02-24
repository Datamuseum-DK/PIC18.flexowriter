// Firmware framework for USB I/O on PIC 18F2455 (and siblings)
// Copyright (C) 2005 Alexander Enzmann
// Copyright (C) 2010 Poul-Henning Kamp
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
// Significantly renovated, generalized and tested on pic18f25j50 by
// Poul-Henning Kamp

// #include <pic18fregs.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "usb.h"

/***********************************************************************
 * DBUG_LEVL bitmasks to turn on/off verbosity 
 *   Excessive debug use may cause USB Congestion .. YMMV 
 */
#define DBUG_IRQ	0x80
#define DBUG_STA	0x40
#define DBUG_UDAT 0x20  
#define DBUG_UCFG 0x10  
#define DBUG_UGEN 0x08  
#define DBUG_UNU2 0x04  
#define DBUG_UNU1 0x02  
#define DBUG_FAIL 0x01  

uint8_t debugLevel = 0x00;

#define DBG(lvl, ...)					\
	do {						\
		if (debugLevel & lvl) {			\
			printf(__VA_ARGS__);		\
			putchar('\r');			\
			putchar('\n');			\
		}					\
	} while (0)

/***********************************************************************/

#ifndef CTASSERT                /* Allow lint to override */
#define CTASSERT(x)             _CTASSERT(x, __LINE__)
#define _CTASSERT(x, y)         __CTASSERT(x, y)
#define __CTASSERT(x, y)        typedef char __assert ## y[(x) ? 1 : -1]
#endif

/***********************************************************************/

#define ALLOW_SUSPEND 0

// Global variables
uint8_t deviceState;
uint8_t remoteWakeup;
static uint8_t deviceAddress;
static uint8_t selfPowered;
uint8_t currentConfiguration;

// Control Transfer Stages - see USB spec chapter 5
#define SETUP_STAGE    0 // Start of a control transfer (followed by 0 or more data stages)
#define DATA_OUT_STAGE 1 // Data from host to device
#define DATA_IN_STAGE  2 // Data from device to host
#define STATUS_STAGE   3 // Unused - if data I/O went ok, then back to Setup

static uint8_t ctrlTransferStage; // Holds the current stage in a control transfer
static uint8_t requestHandled;    // Set to 1 if request was understood and processed.

static const volatile uint8_t *outPtr;	// Data to send to the host
static volatile uint8_t *inPtr;		// Data from the host
static uint16_t wCount;			// Number of bytes of data

#define NewState(state)					\
	do {						\
		deviceState = state;			\
		DBG(DBUG_STA, "->" #state);		\
	} while (0)

/***********************************************************************
 * Pick up descriptors from usb_desc.c
 */

#define W16(x)	((x) & 0xff), ((uint16_t)(x) >> 8)

#include "usb_desc.c"

#undef W16

#define N_STRING (sizeof(stringDescriptors)/sizeof(stringDescriptors[0]))

/***********************************************************************
 * Buffer Descriptors.
 */

struct BDT {
	uint8_t		Stat;
	uint8_t		Cnt;
	uint16_t	Addr;
};

#pragma udata usb4 BDTable
volatile struct BDT __at(0x400) BDTable[32];

#define BDTo(n)	(BDTable[0 + 2 * (n)])
#define BDTi(n)	(BDTable[1 + 2 * (n)])

/***********************************************************************
 * Pipe Buffers
 * Buffer sizes are set in usb_desc.c
 * XXX: 7 more pipes possible
 */

#pragma udata usb4 SetupPacket
static volatile setupPacketStruct SetupPacket;

#pragma udata usb4 controlTransferBuffer
static volatile uint8_t controlTransferBuffer[PIPE_0_SZ_OUT];

#define QUIET(x) (x ? x : 1)	// Silence a silly compiler warning 

#pragma udata usb5 pipe_in_1 
#pragma udata usb5 pipe_in_2 
#pragma udata usb5 pipe_in_3 
#pragma udata usb5 pipe_in_4 
#pragma udata usb5 pipe_in_5 
#pragma udata usb5 pipe_in_6 
#pragma udata usb5 pipe_in_7 
#pragma udata usb5 pipe_in_8 

static volatile uint8_t pipe_in_1[QUIET(PIPE_1_SZ_IN)];
static volatile uint8_t pipe_in_2[QUIET(PIPE_2_SZ_IN)];
static volatile uint8_t pipe_in_3[QUIET(PIPE_3_SZ_IN)];
static volatile uint8_t pipe_in_4[QUIET(PIPE_4_SZ_IN)];
static volatile uint8_t pipe_in_5[QUIET(PIPE_5_SZ_IN)];
static volatile uint8_t pipe_in_6[QUIET(PIPE_6_SZ_IN)];
static volatile uint8_t pipe_in_7[QUIET(PIPE_7_SZ_IN)];
static volatile uint8_t pipe_in_8[QUIET(PIPE_8_SZ_IN)];

static const volatile uint8_t * const pipe_in[] = {
	0,
	pipe_in_1, pipe_in_2, pipe_in_3, pipe_in_4,
	pipe_in_5, pipe_in_6, pipe_in_7, pipe_in_8,
};

static const uint8_t pipe_in_len[] = {
	0,
	PIPE_1_SZ_IN, PIPE_2_SZ_IN, PIPE_3_SZ_IN, PIPE_4_SZ_IN,
	PIPE_5_SZ_IN, PIPE_6_SZ_IN, PIPE_7_SZ_IN, PIPE_8_SZ_IN,
};

#pragma udata usb6 pipe_out_1 
#pragma udata usb6 pipe_out_2 
#pragma udata usb6 pipe_out_3 
#pragma udata usb6 pipe_out_4 
#pragma udata usb6 pipe_out_5 
#pragma udata usb6 pipe_out_6 
#pragma udata usb6 pipe_out_7 
#pragma udata usb6 pipe_out_8 

static volatile uint8_t pipe_out_1[QUIET(PIPE_1_SZ_OUT)];
static volatile uint8_t pipe_out_2[QUIET(PIPE_2_SZ_OUT)];
static volatile uint8_t pipe_out_3[QUIET(PIPE_3_SZ_OUT)];
static volatile uint8_t pipe_out_4[QUIET(PIPE_4_SZ_OUT)];
static volatile uint8_t pipe_out_5[QUIET(PIPE_5_SZ_OUT)];
static volatile uint8_t pipe_out_6[QUIET(PIPE_6_SZ_OUT)];
static volatile uint8_t pipe_out_7[QUIET(PIPE_7_SZ_OUT)];
static volatile uint8_t pipe_out_8[QUIET(PIPE_8_SZ_OUT)];

static const volatile uint8_t * const pipe_out[] = {
	0,
	pipe_out_1, pipe_out_2, pipe_out_3, pipe_out_4,
	pipe_out_5, pipe_out_6, pipe_out_7, pipe_out_8,
};

static const uint8_t pipe_out_len[] = {
	0,
	PIPE_1_SZ_OUT, PIPE_2_SZ_OUT, PIPE_3_SZ_OUT, PIPE_4_SZ_OUT,
	PIPE_5_SZ_OUT, PIPE_6_SZ_OUT, PIPE_7_SZ_OUT, PIPE_8_SZ_OUT,
};

#undef QUIET

/***********************************************************************/

#define BDT_handover(b)				\
	do {					\
		uint8_t s = (b).Stat;		\
		s &= (UOWN | DTS | DTSEN);	\
		s |= (UOWN |       DTSEN);	\
		s ^= (       DTS        );	\
		(b).Stat = s;			\
	} while (0)

/***********************************************************************
 * Send up to len bytes to the host.  The actual number of bytes sent 
 * is returned to the caller.  If the send failed (usually because a
 * send was attempted while the SIE was busy processing the last 
 * request), then 0 is returned.
 */

uint8_t
InPipe(uint8_t pipe, volatile uint8_t *buffer, uint8_t len)
{
	DBG(DBUG_UCFG, "InPipe(%u,,%u)", (uint16_t)pipe, (uint16_t)len);
	// If the CPU still owns the SIE, then don't try to send anything.
	if (BDTi(pipe).Stat & UOWN)
		return 0;
	// Truncate requests that are too large.  TBD: send 
	if(len > pipe_in_len[pipe])
		len = pipe_in_len[pipe];

	// Copy data from user's buffer to dual-ram buffer
	memcpy(pipe_in[pipe], buffer, len);

	BDTi(pipe).Cnt = len;
	BDT_handover(BDTi(pipe));
	return (len);
}

/***********************************************************************
 * Read up to len bytes from output buffer.  Actual number of bytes
 * put into buffer is returned.  If there are fewer than len bytes, then
 * only the available bytes will be returned.  Any bytes in the buffer
 * beyond len will be discarded.
 */

uint8_t
OutPipe(uint8_t pipe, volatile uint8_t *buffer, uint8_t len)
{

	// We can only pull data if we own the buffer
	if(BDTo(pipe).Stat & UOWN)
		return (0);

	DBG(DBUG_UCFG, "OP(%u,,%u)", (uint16_t)pipe, (uint16_t)len);

	// See if the host sent fewer bytes that we asked for.
	if(len > BDTo(pipe).Cnt)
		len = BDTo(pipe).Cnt;

	// Copy data from dual-ram buffer to user's buffer
	memcpy(buffer, pipe_out[pipe], len);

	// Reset the output buffer descriptor so the host
	// can send more data.
	BDTo(pipe).Cnt = pipe_out_len[pipe];
	BDT_handover(BDTo(pipe));
	return (len);
}

/***********************************************************************
 * After configuration is complete, this routine is called to initialize
 * the endpoints (e.g., assign buffer addresses).
 */

/*lint -e{415,416} */
#if 0
static void
InitPipe(uint8_t pipe)
{
	__sfr *uep = &UEP0 + pipe;

	if (pipe_in_len[pipe] == 0 && pipe_out_len[pipe] == 0)
		return;
	DBG(DBUG_UCFG, "InitPipe(%u)", pipe);

	// Turn on both in and out for this endpoint
	*uep = 0x18;
	if (pipe_in_len[pipe])
		*uep |= 0x02;
	if (pipe_out_len[pipe])
		*uep |= 0x04;

	BDTo(pipe).Cnt = pipe_out_len[pipe];
	BDTo(pipe).Addr = PTR16(pipe_out[pipe]);
	BDTo(pipe).Stat = UOWN | DTSEN;

	BDTi(pipe).Addr = PTR16(pipe_in[pipe]);
	BDTi(pipe).Stat = DTS;
}
#else
#define InitPipe(pipe)						\
	do {							\
		__sfr *uep = &UEP0 + pipe;			\
		if (pipe_in_len[pipe] != 0 || pipe_out_len[pipe] != 0) { \
			DBG(DBUG_UCFG, "InitPipe(%u)", pipe);	\
								\
			/* Turn on both in and out for this endpoint */	\
			*uep = 0x18;				\
			if (pipe_in_len[pipe])			\
				*uep |= 0x02;			\
			if (pipe_out_len[pipe])			\
				*uep |= 0x04;			\
								\
			BDTo(pipe).Cnt = pipe_out_len[pipe];	\
			BDTo(pipe).Addr = PTR16(pipe_out[pipe]);	\
			BDTo(pipe).Stat = UOWN | DTSEN;		\
								\
			BDTi(pipe).Addr = PTR16(pipe_in[pipe]);	\
			BDTi(pipe).Stat = DTS;			\
		}						\
	} while (0)

#endif

static struct linecoding {
	uint32_t	speed;
	uint8_t		stop;
	uint8_t		parity;
	uint8_t		databits;
} CDC_linecoding;

static uint8_t CDC_modem = 0;

// Process CDC specific requests
static void
ProcessCDCRequest(void)
{

	if (SetupPacket.bmRequestType != 0x21)
		return;
	if (SetupPacket.bRequest == 0x22) {
		CDC_modem = SetupPacket.wValue0;
		requestHandled = 1;
	} else if (SetupPacket.bRequest == 0x20) {
		inPtr = (void*)&CDC_linecoding;
		requestHandled = 1;
	}
}

static void
CDC_Callback(void)
{
	
	printf("s=%lu %d %d %d\r\n",
	    CDC_linecoding.speed,
	    CDC_linecoding.stop,
	    CDC_linecoding.parity,
	    CDC_linecoding.databits);
}

//
// Start of code to process standard requests (USB chapter 9)
//

// Process GET_DESCRIPTOR
static void
GetDescriptor(void)
{
	uint8_t descriptorType  = SetupPacket.wValue1;
	uint8_t descriptorIndex = SetupPacket.wValue0;

	DBG(DBUG_UCFG, "GetDesc(%x,%x)", descriptorType, descriptorIndex);

	if (descriptorType == DEVICE_DESCRIPTOR) {
		requestHandled = 1;
		outPtr = deviceDescriptor;
		wCount = *(outPtr + 0);
	} else if (descriptorType == CONFIGURATION_DESCRIPTOR) {
		requestHandled = 1;
		outPtr = configDescriptor;
		wCount = *(outPtr + 2);
	} else if (descriptorType == STRING_DESCRIPTOR) {
		requestHandled = 1;
		if (descriptorIndex >= N_STRING)
			descriptorIndex = N_STRING - 1;
		outPtr = stringDescriptors[descriptorIndex];
		wCount = *outPtr;
	}
}

// Process GET_STATUS
static void
GetStatus(void)
{
	uint8_t c0 = 0, c1 = 0;
	// Mask off the Recipient bits
	uint8_t recipient = SetupPacket.bmRequestType & 0x1F;
	DBG(DBUG_UGEN, "GetStatus");
	// See where the request goes
	if (recipient == 0x00) {
		// Device
		requestHandled = 1;
		// Set bits for self powered device and remote wakeup.
		if (selfPowered)
			c0 |= 0x01;
		if (remoteWakeup)
			c0 |= 0x02;
	} else if (recipient == 0x01) {
		// Interface
		requestHandled = 1;
	} else if (recipient == 0x02) {
		// Endpoint
		uint8_t endpointNum = SetupPacket.wIndex0 & 0x0F;
		uint8_t endpointDir = SetupPacket.wIndex0 & 0x80;
		requestHandled = 1;
		if (endpointDir) {
			if (BDTi(endpointNum).Stat & BSTALL)
				c0 = 0x01;
		} else {
			if (BDTo(endpointNum).Stat & BSTALL)
				c0 = 0x01;
		}
	}
	if (requestHandled) {
		outPtr = controlTransferBuffer;
		controlTransferBuffer[0] = c0;
		controlTransferBuffer[1] = c1;
		wCount = 2;
	}
}

/***********************************************************************
 * SET_FEATURE & CLEAR_FEATURE
 */

static void
SetFeature(void)
{
	uint8_t recipient = SetupPacket.bmRequestType & 0x1F;
	uint8_t feature = SetupPacket.wValue0;
	DBG(DBUG_UCFG, "SetFeature(%x,%x)", recipient, feature);
	if (recipient == 0x00) {
		// Device
		if (feature == DEVICE_REMOTE_WAKEUP) {
			requestHandled = 1;
			if (SetupPacket.bRequest == SET_FEATURE)
				remoteWakeup = 1;
			else
				remoteWakeup = 0;
		}
		// TBD: Handle TEST_MODE
	} else if (recipient == 0x02) {
		// Endpoint
		uint8_t endpointNum = SetupPacket.wIndex0 & 0x0F;
		uint8_t endpointDir = SetupPacket.wIndex0 & 0x80;
		uint8_t c;

		if ((feature == ENDPOINT_HALT) && (endpointNum != 0)) {
			// Halt endpoint (as long as it isn't endpoint 0)
			requestHandled = 1;
			if(SetupPacket.bRequest == SET_FEATURE)
				c = 0x84;
			else {
				if(endpointDir)
					c = 0x00;
				else
					c = 0x88;
			}
			if (endpointDir)
				BDTi(endpointNum).Stat = c;
			else
				BDTo(endpointNum).Stat = c;
		}
	}
}

// Data stage for a Control Transfer that sends data to the host
static void
InDataStage(void)
{
	uint16_t bufferSize;
	// Determine how many bytes are going to the host
	if(wCount < sizeof controlTransferBuffer)
		bufferSize = wCount;
	else 
		bufferSize = sizeof controlTransferBuffer;
	// DBG(DBUG_UCFG, "IDS");
	// Load the high two bits of the byte count into BC8:BC9
	BDTi(0).Stat &= ~(BC8 | BC9); // Clear BC8 and BC9
	BDTi(0).Stat |= (uint8_t)((bufferSize & 0x0300) >> 8);
	BDTi(0).Cnt = (uint8_t)(bufferSize & 0xFF);
	BDTi(0).Addr = PTR16(&controlTransferBuffer);
	// Update the number of bytes that still need to be sent.  Getting
	// all the data back to the host can take multiple transactions, so
	// we need to track how far along we are.
	wCount = wCount - bufferSize;
	// Move data to the USB output buffer from wherever it sits now.
	inPtr = controlTransferBuffer;

	memcpy(inPtr, outPtr, bufferSize);
}

// Data stage for a Control Transfer that reads data from the host
static void
OutDataStage(void)
{
	uint16_t bufferSize;
	bufferSize = ((0x03 & BDTo(0).Stat) << 8) | BDTo(0).Cnt;

	DBG(DBUG_UCFG, "OutDataStage: %u", bufferSize);
	// Accumulate total number of bytes read
	wCount = wCount + bufferSize;
	outPtr = controlTransferBuffer;
	memcpy(inPtr, outPtr, bufferSize);
}

// Configures the buffer descriptor for endpoint 0 so that it is waiting for
// the status stage of a control transfer.
static void
WaitForSetupStage(void)
{
	ctrlTransferStage = SETUP_STAGE;
	BDTo(0).Cnt = sizeof SetupPacket;
	BDTo(0).Addr = PTR16(&SetupPacket);
	BDTo(0).Stat = UOWN | DTSEN; // Give to SIE, enable data toggle checks
	BDTi(0).Stat = 0x00;         // Give control to CPU
}

void
DisableUSBModule(void)
{

	UCONbits.SUSPND = 0;
	UCON = 0;
	NewState(DETACHED);
}

void
EnableUSBModule(void)
{
	if(UCONbits.USBEN == 0) {
		UCON = 0;
		UIE = 0;
		UCONbits.USBEN = 1;
		NewState(ATTACHED);
	}
	// If we are attached and no single-ended zero is detected, then
	// we can move to the Powered state.
	if ((deviceState == ATTACHED) && !UCONbits.SE0) {
		UIR = 0;
		UIE = 0;
		UIEbits.URSTIE = 1;
		UIEbits.IDLEIE = 1;
		UIEbits.ACTVIE = 1;
		NewState(POWERED);
	}
}

/**********************************************************************
 * Start Of Frame token received
 */

static void
StartOfFrame(void)
{

	// DBG(DBUG_UNU1, "<SOF>");
}

// This routine is called in response to the code stalling an endpoint.
static void
Stall(void)
{
	if(UEP0bits.EPSTALL == 1) {
		// Prepare for the Setup stage of a control transfer
		WaitForSetupStage();
		UEP0bits.EPSTALL = 0;
	}
}

/**********************************************************************
 * Suspend/Resume
 */

// Unsuspend the device
static void
Resume(void)
{

	UCONbits.SUSPND = 0;
	UIEbits.ACTVIE = 0;
}

// Suspend all processing until we detect activity on the USB bus
static void
Suspend(void)
{

	UIEbits.ACTVIE = 1;
	UCONbits.SUSPND = 1;

#if ALLOW_SUSPEND
	DBG(DBUG_UNU1, "Suspend");
	UIEbits.ACTVIE = 1;
	UIRbits.IDLEIF = 0;
	UCONbits.SUSPND = 1;

	PIR2bits.USBIF = 0;
	INTCONbits.RBIF = 0;
	PIE2bits.USBIE = 1;
	INTCONbits.RBIE = 1;

	// disable the USART
	RCSTAbits.CREN = 0;
	TXSTAbits.TXEN = 0;

	Sleep();

	// enable the USART
	RCSTAbits.CREN = 1;
	TXSTAbits.TXEN = 1;

	PIE2bits.USBIE = 0;
	INTCONbits.RBIE = 0;
#endif
}

/**********************************************************************
 * Bus Reset
 */

static void
BusReset()
{

	UEIR  = 0x00;		// Clear all errors
	UIR   = 0x00;		// Clear all interrupts
	UEIE  = 0x9f;		// Enable all errors
	UIE   = 0x3b;		// Enable interrupts but ACTIVEF

	UADDR = 0x00;		// Default address

	/* EP0 is control, disable the rest */
	UEP0 = 0x16;	UEP4=0x00;	UEP8=0x00;	UEP12=0x00;
	UEP1=0x00;	UEP5=0x00;	UEP9=0x00;	UEP13=0x00;
	UEP2=0x00;	UEP6=0x00;	UEP10=0x00;	UEP14=0x00;
	UEP3=0x00;	UEP7=0x00;	UEP11=0x00;	UEP15=0x00;

	// Flush any pending transactions
	while (UIRbits.TRNIF == 1)
		UIRbits.TRNIF = 0;

	// Enable packet processing
	UCONbits.PKTDIS = 0;

	// Prepare for the Setup stage of a control transfer
	WaitForSetupStage();

	remoteWakeup = 0;		// Remote wakeup is off by default
	selfPowered = 0;		// Self powered is off by default
	currentConfiguration = 0;	// Clear active configuration
	NewState(DEFAULT);
}

/***********************************************************************
 * Main entry point for USB tasks.
 * Checks interrupts, then checks for transactions.
 */

void
USB_intr(void) 
{   

	if (UIR || USTAT)
		DBG(DBUG_IRQ, "<%x,%x>", UIR, USTAT);

	PIR2bits.USBIF = 0;

	// See if the device is connected yet.
	if(deviceState == DETACHED) {
		DBG(DBUG_UNU1, "ProcUSBTxcts() DvStat == DETACHED exit");
		return;
	}
	// If the USB became active then wake up from suspend
	if(UIRbits.ACTVIF) {
		Resume();
		while (UIRbits.ACTVIF)
			UIRbits.ACTVIF = 0;
		DBG(DBUG_UNU1, "<resume>");
	}  
	// If we are supposed to be suspended, then don't try performing any
	// processing.
	if(UCONbits.SUSPND == 1) {
		DBG(DBUG_UNU1,
		    "ProcUSBTxcts() UCON:x%hx SUSPND ==1; exit", UCON);
		return;
	}
	// Process a bus reset
	if (UIRbits.URSTIF) {
		BusReset();
		DBG(DBUG_UNU1, "<busreset>");
	}
	if (UIRbits.IDLEIF) {
		// No bus activity for a while - suspend the firmware
		UIRbits.IDLEIF = 0;
		Suspend();
		DBG(DBUG_UNU1, "<suspend>");
	}

	if (UIRbits.SOFIF) {
		StartOfFrame();
		UIRbits.SOFIF = 0;
	}

	if (UIRbits.STALLIF) {
		DBG(DBUG_UNU1, "<stall>");
		Stall();
		UIRbits.STALLIF = 0;
	}
	if (UIRbits.UERRIF) {
		// TBD: See where the error came from.
		DBG(DBUG_UNU1, "Error %x", UEIR);
		UEIR = 0;
		// Clear errors
		UIRbits.UERRIF = 0;
	}

	if (deviceState < DEFAULT) 
		return;

	if(!UIRbits.TRNIF)
		return;

	while(UIRbits.TRNIF) {
		uint8_t PID;
		uint8_t request;
		uint8_t reqtyp;

		/*
		 * Transaction finished Interrupt
		 */
		if (USTAT == 0x04) {
			if ((UADDR == 0) && (deviceState == ADDRESS)) {
				// TBD: ensure that the new address matches the value of
				// "deviceAddress" (which came in through a SET_ADDRESS).
				UADDR = SetupPacket.wValue0;
				if(UADDR == 0) {
					// If we get a reset after a SET_ADDRESS,
					// then we need to drop back to the Default
					// state.
					NewState(DEFAULT);
				}  
			}
			if (ctrlTransferStage == DATA_IN_STAGE) {
				// Start (or continue) transmitting data
				InDataStage();
				BDT_handover(BDTi(0));
			} else {
				// Prepare for the Setup stage of a control transfer
				WaitForSetupStage();
			}
			UIRbits.TRNIF = 0;
			continue;
		}
		if (USTAT != 0) {
			DBG(DBUG_UNU1, "<TRNIF ustat=%x>", USTAT);
			UIRbits.TRNIF = 0;
			continue;
		}
		/***********************************************************************
		 * This is the starting point for processing a Control Transfer.
		 * The code directly follows the sequence of transactions described in
		 * the USB spec chapter 5.  The only Control Pipe in this firmware is the
		 * Default Control Pipe (endpoint 0).
		 * Control messages that have a different destination will be discarded.
		 */

		// Endpoint 0:out
		PID = (BDTo(0).Stat & 0x3C) >> 2;
		if (PID == 0x0D) {
			// SETUP PID - a transaction is starting
			// Process the Setup stage of a control transfer.  This code initializes the
			// flags that let the firmware know what to do during subsequent stages of
			// the transfer.
			//static void
			//SetupStage(void)
			// Note: Microchip says to turn off the UOWN bit on the IN direction as
			// soon as possible after detecting that a SETUP has been received.
			BDTi(0).Stat &= ~UOWN;
			BDTo(0).Stat &= ~UOWN;

			request = SetupPacket.bRequest;
			reqtyp = SetupPacket.bmRequestType;

			// Initialize the transfer process
			ctrlTransferStage = SETUP_STAGE;
			requestHandled = 0; // Default is that request hasn't been handled
			wCount = 0;         // No bytes transferred
			// See if this is a standard (as definded in USB chapter 9) request
			//static void
			//ProcessStandardRequest(void)

			if((reqtyp & 0x60) != 0x00) {
				// Not a standard request - don't process here.  Class or Vendor
				// requests have to be handled seperately.
			} else if (request == SET_ADDRESS) {
				// Set the address of the device.  All future requests
				// will come to that address.  Can't actually set UADDR
				// to the new address yet because the rest of the SET_ADDRESS
				// transaction uses address 0.
				DBG(DBUG_UDAT, "<addr=%u>", SetupPacket.wValue0);
				requestHandled = 1;
				NewState(ADDRESS);
				deviceAddress = SetupPacket.wValue0;
			} else if (request == GET_DESCRIPTOR && reqtyp == 0x80) {
				GetDescriptor();
			} else if (request == SET_CONFIGURATION) {
				requestHandled = 1;
				currentConfiguration = SetupPacket.wValue0;
				DBG(DBUG_UDAT, "SetConf(%d)", currentConfiguration);
				// TBD: ensure the new configuration value is one that
				// exists in the descriptor.
				if (currentConfiguration == 0) {
					// If configuration value is zero, device is put in
					// address state (USB 2.0 - 9.4.7)
					NewState(ADDRESS);
				} else {
					char i;
					// Set the configuration.
					NewState(CONFIGURED);

					for (i = 1; i < 9; i++)
						InitPipe(i);
				}
			} else if (request == GET_CONFIGURATION) {
				DBG(DBUG_UCFG, "GET_CONFIGURATION");
				requestHandled = 1;
				outPtr = (uint8_t*)&currentConfiguration;
				wCount = 1;
			} else if (request == GET_STATUS) {
				GetStatus();
			} else if ((request == CLEAR_FEATURE) || (request == SET_FEATURE)) {
				SetFeature();
			} else if (request == GET_INTERFACE) {
				// No support for alternate interfaces.  Send
				// zero back to the host.
				DBG(DBUG_UCFG, "GET_INTERFACE");
				requestHandled = 1;
				controlTransferBuffer[0] = 0;
				//typecast below got it working with SDCC 2.8.0 
				outPtr = controlTransferBuffer;
				wCount = 1;
			} else if (request == SET_INTERFACE) {
				// No support for alternate interfaces - just ignore.
				DBG(DBUG_UCFG, "SET_INTERFACE");
				requestHandled = 1;
			} else if (request == SET_DESCRIPTOR) {
				DBG(DBUG_UCFG, "SET_DESCRIPTOR");
			} else if (request == SYNCH_FRAME) {
				DBG(DBUG_UCFG, "SYNCH_FRAME");
			} else {
				DBG(DBUG_UCFG, "Default Std Request");
			}
			// See if the HID class can do something with it.
			if (!requestHandled)
				ProcessCDCRequest();
			// TBD: Add handlers for any other classes/interfaces in the device
			if (!requestHandled) {
				// If this service wasn't handled then stall endpoint 0
				BDTo(0).Cnt = sizeof SetupPacket;
				BDTo(0).Addr = PTR16(&SetupPacket);
				BDTo(0).Stat = UOWN | BSTALL;
				BDTi(0).Stat = UOWN | BSTALL;
			} else if (reqtyp & 0x80) {
				// Device-to-host
				if(SetupPacket.wLength < wCount)
					wCount = SetupPacket.wLength;
				InDataStage();
				ctrlTransferStage = DATA_IN_STAGE;
				// Reset the out buffer descriptor for endpoint 0
				BDTo(0).Cnt = sizeof SetupPacket;
				BDTo(0).Addr = PTR16(&SetupPacket);
				BDTo(0).Stat = UOWN;
				// Set the in buffer descriptor on endpoint 0 to send data
				// BDTi(0).Addr = PTR16(&controlTransferBuffer);
				// Give to SIE, DATA1 packet, enable data toggle checks
				BDTi(0).Stat = UOWN | DTS | DTSEN; 
			} else {
				// Host-to-device
				ctrlTransferStage = DATA_OUT_STAGE;
				// Clear the input buffer descriptor
				BDTi(0).Cnt = 0;
				BDTi(0).Stat = UOWN | DTS | DTSEN;
				// Set the out buffer descriptor on endpoint 0 to receive data
				BDTo(0).Cnt = sizeof controlTransferBuffer;
				BDTo(0).Addr = PTR16(&controlTransferBuffer);
				// Give to SIE, DATA1 packet, enable data toggle checks
				BDTo(0).Stat = UOWN | DTS | DTSEN;
			}
			// Enable SIE token and packet processing
			UCONbits.PKTDIS = 0;
		} else if (ctrlTransferStage == DATA_OUT_STAGE) {
			// Complete the data stage so that all information has
			// passed from host to device before servicing it.
			OutDataStage();
			// If this is a HID request,
			// then invoke the callback to handle
			// the control out (when necessary).
			CDC_Callback();
			BDT_handover(BDTo(0));
		} else {
			// Prepare for the Setup stage of a control transfer
			WaitForSetupStage();
		}
		UIRbits.TRNIF = 0;
	}
}
