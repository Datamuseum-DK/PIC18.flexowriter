/*-
 * Copyright (c) 2005 Poul-Henning Kamp
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: phk_flexowriter_Input_works.c,v 1.1 2011/05/19 13:57:37 phk Exp $
 *
 */

#ifdef FLEXELINT
typedef unsigned char __sfr;
#endif

#define SERIAL	1

#define USB 0

#include "pic18fregs.h"

#include <stdio.h>
#include <stdint.h>


#ifndef CTASSERT                /* Allow lint to override */
#define CTASSERT(x)             _CTASSERT(x, __LINE__)
#define _CTASSERT(x, y)         __CTASSERT(x, y)
#define __CTASSERT(x, y)        typedef char __assert ## y[(x) ? 1 : -1]
#endif

#pragma stack 0x200 0x100

__code unsigned char __at(__CONFIG6H) config6h = 0
	| (1 << 6)	// WRTB
	;

__code unsigned char __at(__CONFIG4L) config4l = 0
	| (1 << 7)	// DEBUG (disable)
	| (0 << 6)	// XINST (disable)
	| (0 << 5)	// ICPRT (disable)
	| (1 << 2)	// LVP (enable)
	| (1 << 0)	// STVREN (enable)
	;

__code unsigned char __at(__CONFIG3H) config3h = 0
	| (1 << 7)	// MCLR (enable)
	| (0 << 1)	// PBADEN (digital)
	;

/* Enable the watchdog at 4msec * 64 = .256 sec */
__code unsigned char __at(__CONFIG2H) config2h = 0
	| (0xc << 1)	// WDTPS (1:4096)
	| (1 << 0)	// WDTEN (enable)
	;

__code unsigned char __at(__CONFIG2L) config2l = 0
	| (1 << 5)	// VREGEN (enable)
	| (3 << 3)	// BORV (min setting)
	| (3 << 1)	// BOREN (hw only)
	| (1 << 0)	// PWRTEN (disable)
	;

/* No code protects.  No CPU system clock divde */
__code unsigned char __at(__CONFIG1H) config1h = 0
	| (1 << 7)	// IESO
	| (1 << 6)	// FCMEN
	| (0xe << 0)	// FOSC: HSPLL
	;

/* No debugger, no extins, reset on stack, WDT enable, PLLDIV=? */
__code unsigned char __at(__CONFIG1L) config1l = 0
	| (1 << 5)	// USBDIV
	| (0 << 3)	// CPUDIV: 96MHz / 2 = 48MHz
	| (4 << 0)	// PLLDIV: Divide by 5 (20MHz Xtal)
	;

#define MHZ	24		// Just so we remember

/* Serial port defines -----------------------------------------------*/

#if USB
#include "usb.c"
#endif

#if SERIAL

#define SERIALPORTS (0 << 0)

#include "serial.c"

/*********************************************************************/
static uint8_t pcbuf[256];
static volatile uint8_t pcbuf_r;
static volatile uint8_t pcbuf_w;
static volatile uint8_t pcoflo;

#define pc_poll()						\
	do {							\
		uint8_t c;					\
								\
		if (SERIAL_TXRDY()) {				\
			if (pcbuf_r == pcbuf_w && pcoflo) {	\
				SERIAL_TX(, '!');		\
				pcoflo = 0;			\
				PIE1bits.TXIE = 1;		\
			} else if (pcbuf_r != pcbuf_w) {	\
				c = pcbuf[pcbuf_r++];		\
				SERIAL_TX(, c);			\
				PIE1bits.TXIE = 1;		\
			} else {				\
				PIE1bits.TXIE = 0;		\
			}					\
		}						\
	} while (0)

void
putchar(char c) __wparam
{
	volatile uint8_t i, j;

	j = INTCON;
	i = pcbuf_w;
	if (i + 1 == pcbuf_r) 
		pcoflo = 1;
	if (!pcoflo) {
		pcbuf[i] = c;
		// pcbuf[i] = (STKPTR & 0x1f) + 0x40;
		pcbuf_w = i + 1;
	}
	pc_poll();
	INTCON = j;
}

#endif

#if USB
/*********************************************************************/
static uint8_t txBuffer[64];
static uint8_t txbp;
static uint8_t rxBuffer[64];
static uint8_t rxbp, rxbe;
static uint8_t loop;

static uint8_t hmode;

static uint16_t rate;

static const uint8_t hex[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

/* XXX: move buff er insertion after strobe timing ? */
static void
dochar(void)
{
	uint8_t c, u;

	if (txbp + 1 + hmode * 3 > sizeof txBuffer) 
		return;

	if (!PORTCbits.RC7)
		return;

	if ((CDC_modem & 3) != 3)		/* DTR + RTS */
		return;

	c = PORTB;
	/* XXX: validation read, to check PORTB bits are stable ? */
	PORTCbits.RC6 = 0;
	for (u = 0; u < 10; u++)
		if(!PORTCbits.RC7)
			break;
	if (hmode) {
		txBuffer[txbp++] = hex[((c & 0xf0) >> 4)];
		txBuffer[txbp++] = hex[(c & 0xf)];
		txBuffer[txbp++] = '\r';
		txBuffer[txbp++] = '\n';
	} else {
		txBuffer[txbp++] = c;
	}
	/* XXX: calibrate width of strobe pulse */
	for (u = 0; u < 30; u++)		/* 41.6 miuroseconds */
		;
	PORTCbits.RC6 = 1;
}

static const char usage[] =
	"\r\n\n"
	"RC-2000 USB adapter\r\n"
	"===================\r\n"
	"0:\tSingle Step\r\n"
	"1-9:\tSet Speed\r\n"
	"+:\tFaster\r\n"
	"-:\tSlower\r\n"
	"b:\tBinary mode\r\n"
	"h:\tHex mode\r\n"
	"?:\tThis help\r\n"
	"\n"
	"2010-02-20 Poul-Henning Kamp\r\n"
	"\n";

static void
Send(void)
{
	uint8_t j;

	j = InPipe(1, txBuffer, txbp);
	if (j == 0) {
	} else if (j == txbp) {
		txbp = 0;
	} else {
		printf_tiny("Sent %u/%u\n\r", (uint16_t)j, (uint16_t)txbp);
		txbp = 0;
	}
}

// Regardless of what the USB is up to, we check the USART to see
// if there's something we should be doing.
static void
USBEcho(void)
{
	uint8_t j;
	uint16_t x;

#if SERIALz
	uint8_t rxByte;

	if (serial_rxoerr(2)) {
		serial_init(2);
	}
	if (serial_rxrdy(2)) {
		rxByte = serial_rx(2);
		if (txbp < sizeof txBuffer)
			txBuffer[txbp++] = rxByte;
	}
#endif
	if ((deviceState < CONFIGURED) || (UCONbits.SUSPND == 1)) 
		return;

	if (!(CDC_modem & 1)) {		/* DTR */
		txbp = 0;
		rate = 0;
	}

	x = TMR0L; 
	x |= (TMR0H << 8);
	if (rate > 0 && x > rate) {
		TMR0H = 0;
		TMR0L = 0;
		dochar();
	}
	loop++;
	if (txbp == 64) 
		Send();

	if (loop)
		return;

	if (txbp > 0) 
		Send();

	if (rxbp < rxbe) {
		j = rxBuffer[rxbp];
		printf_tiny("%c", j);
		// if (txbp < sizeof txBuffer) txBuffer[txbp++] = j;
		switch (j) {
		case 'b':
			hmode = 0;
			break;
		case 'h':
			hmode = 1;
			break;
		case '0':
			rate = 0;
			dochar();
			break;
#define T0HZ	750000UL
		case '1': rate = 15000; break;	// 50 cps
		case '2': rate =  7500; break;	// 100 cps
		case '3': rate =  3750; break;	// 200 cps
		case '4': rate =  1500; break;	// 380 cps
		case '5': rate =   750; break;	// 718 cps
		case '6': rate =   600; break;	// 1034 cps
		case '7': rate =   400; break;	// 1411 cps
		case '8': rate =   300; break;	// 1780 cps
		case '9': rate =    16; break;	// 2479 cps
		case '-':
			x = rate + (rate >> 4);
			if (x > rate)
				rate = x;
			else
				rate = 0;
			break;
		case '+':
			if (rate == 0)
				rate = 65500U;
			else {
				x = rate - (rate >> 4);
				if (x < rate)
					rate = x;
			}
			break;
		case '?':
			for (j = 0; j < sizeof usage; j++) {
				txBuffer[txbp++] = usage[j];
				while (txbp == sizeof txBuffer)
					Send();
			}
			break;
		default:
			break;
		}
		printf_tiny("Rate = %u\n\r", rate);
		rxbp++;
	} else {
		rxbe = OutPipe(1, rxBuffer, sizeof rxBuffer);
		rxbp = 0;
	}
}

#endif

/*********************************************************************/

void
intr_h() __interrupt 1
{
	uint8_t u, j;

	u = INTCON;
	if (u & 0x02) {
		PORTBbits.RB1 = 1;
		// INT0 external int
		INTCONbits.INT0IF = 0;
	}
	PORTDbits.RD3 = 1;
#if USB
	u = PIR2;
	PORTBbits.RB4 = 1;
	if (u & 0x10) {
		PORTDbits.RD2 = 1;
		USB_intr();
		PORTDbits.RD2 = 0;
	}
	PORTBbits.RB4 = 0;
	PIR2 = 0;
#endif
#if SERIAL
	u = PIR1;
	if (u & 0x10) {
		j = INTCON;
		pc_poll();
		INTCON = j;
	}
	PIR1 = 0;
#endif
	PORTDbits.RD3 = 0;
}

/*********************************************************************/

void
main(void) __wparam 
{

	TRISDbits.TRISD1 = 0;
	TRISDbits.TRISD2 = 0;
	TRISDbits.TRISD3 = 0;
#if SERIAL
	SERIAL_INIT();
	SERIAL_BAUD(, 104);		// 48MHz / (4 * (115200 + 1) 
	TXSTAbits.BRGH = 1;
	BAUDCONbits.BRG16 = 1;
	TRISCbits.TRISC6 = 0;
#endif

	stdin = STREAM_USER;
	stdout = STREAM_USER;

	/*
	 * T0 Freq = 48MHz / (4 * 16) = 750 kHz
	 */
	T0CON = 0
	    | (1 << 7)		// Enable
	    | (3 << 0)		// 1:16 Prescaler
	    ;

#if USB
	// Initialize USB
	UCFG = 0x14; // Enable pullup resistors; full speed mode
	deviceState = DETACHED;
	remoteWakeup = 0x00;
	currentConfiguration = 0x00;
#endif 

	{
	unsigned u = 1;
	while (u++)
		;
	}

	INTCON2bits.RBPU = 0;		// Weak pull-up PORTB

	/* Setup Interrupts */
	{
	uint8_t u, v;
	u = RCON;
	v = STKPTR;
	RCON=3;
	RCONbits.IPEN = 1;
#if 0
	printf_tiny("|%x|%x|Hello\n", u, STKPTR);
#endif
	STKPTRbits.STKFUL = 0;
	STKPTRbits.STKUNF = 0;
	}
	INTCON = 0xc0;
	putchar('\r');
	putchar('\n');
	putchar('\r');
	putchar('\n');
	putchar('H');
	putchar('W');
	putchar('\r');
	putchar('\n');
	while (pcbuf_r != pcbuf_w) 
		;

	{
	uint8_t i, j;
	uint16_t w;
	INTCON2bits.INTEDG0 = 0;	// Which edge
	INTCONbits.INT0IE = 1;
	TRISBbits.TRISB1 = 0;

	PORTEbits.RDPU = 1;

		while (1) {
			ClrWdt();

			if (0) {
				for (w = 0; w < 1000; w++)
					;
				PORTBbits.RB1 = !PORTBbits.RB1;
				continue;
			}



			if (PORTBbits.RB1 == 0)
				continue;

			putchar('A' + (PORTD >> 4));
			// putchar('I');
			for (w = 0; w < 10000; w++)
				;
			PORTBbits.RB1 = 0;
			continue;
				
			i = PORTBbits.RB0;
			if (i == j)
				continue;
			putchar('0' + i);
			j = i;
			if (!j) {
				PORTBbits.RB1 = 0;
			} else {
				//putchar('A' + (PORTD >> 4));
				//putchar(' ');
			}
			
		}
	}

#if 0
	/*************************************************************/
	PORTEbits.RDPU = 1;
	TRISDbits.TRISD4 = 0;
	TRISDbits.TRISD5 = 0;
	while (1) {
		uint8_t x, y;
		uint16_t w;

		PORTDbits.RD4 = 1;
		PORTDbits.RD5 = 1;
		for (x= 0; x < 3; x++)
			for(w = 0; w < 0x8000; w++)
				ClrWdt();
		PORTDbits.RD4 = 0;
		PORTDbits.RD5 = 0;
		for (x= 0; x < 100; x++)
			for(w = 0; w < 0x8000; w++)
				ClrWdt();
		continue;

#if 0		
		

		w = 0;
		x = 0;
		while (1) {
			ClrWdt();
			y = PORTD >> 6;
			if (y != x) 
				putchar(0x40 | y);
			x = y;
			PORTDbits.RD4 = (w >> 15) & 1;
			PORTDbits.RD5 = (w >> 15) & 1;
			w++;
		}
#endif
	}
#endif

	/*************************************************************/
#if USB	
	PIE1 = 0;
	PIE2 = 0;
	PIE2bits.USBIE = 0;

	rate = 0;
	txbp = 0;
	rxbp = 0;
	rxbe = 0;
	hmode = 1;

	while(1) {
		ClrWdt();
		//INTCON = 0x00;
		if (!PIE2bits.USBIE)
			USB_intr();
		//INTCON = 0xc0;
		// Ensure USB module is available
		PORTDbits.RD1 = 1;
		EnableUSBModule();
		// USBEcho();
		PORTDbits.RD1 = 0;
	}
#endif
}
