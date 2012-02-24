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
 * $Id: phk_flexowriter.c,v 1.7 2011/09/12 19:54:23 phk Exp $
 *
 * XXX:	for output:  PORTbits -> LATAbits ?   througout ?
 * XXX:	why double RA0 = 0 necessary ?
 */

#ifdef FLEXELINT
typedef unsigned char __sfr;
#endif

#define SERIAL	1

#define USB 1

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
		if (PIR1bits.TXIF) {				\
			if (pcbuf_r == pcbuf_w && pcoflo) {	\
				SERIAL_TX(, '!');		\
				pcoflo = 0;			\
				PIE1bits.TXIE = 1;		\
			} else if (pcbuf_r != pcbuf_w) {	\
				c = pcbuf[pcbuf_r++];		\
				SERIAL_TX(, c);			\
				PIE1bits.TXIE = 1;		\
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

static void
Send(void)
{
	uint8_t j;

	if (txbp == 0)
		return;
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

static uint8_t state, istate, lamp;

// Units of 375kHz
static uint16_t dl1 = 37500;	// 100 msec keypress release ready
static uint16_t dl2 = 18750;	//  50 msec keyboard pulse width
static uint16_t dl3 = 37500;	// 100 msec Lamp refresh interval
static uint16_t dl4 =  3000;	// Print delay

static void
USBEcho(void)
{
	uint8_t j;
	uint16_t x;

	if ((deviceState < CONFIGURED) || (UCONbits.SUSPND == 1)) 
		return;

	if (!(CDC_modem & 1)) {		/* DTR */
		txbp = 0;
	}

	x = TMR0L; 
	x |= (TMR0H << 8);
	j = PORTDbits.RD0;
	switch (state) {
	case 0:
		/*
		 * IDLE state
		 */
		if (rxbp == rxbe) {
			/* Check if computer sent us anything */
			rxbe = OutPipe(1, rxBuffer, sizeof rxBuffer);
			rxbp = 0;
		}
		if (rxbp < rxbe) {
			j = rxBuffer[rxbp++];
			printf_tiny("U%x ", j);
			j ^= 0x3f;
			if (j & 0x01)
				PORTCbits.RC1 = 1;
			if (j & 0x02)
				PORTCbits.RC0 = 1;
			if (j & 0x04)
				PORTAbits.RA3 = 1;
			if (j & 0x08)
				PORTAbits.RA2 = 1;
			if (j & 0x10)
				PORTAbits.RA1 = 1;
			if (j & 0x20)
				PORTAbits.RA0 = 1;
			if (j & 0x40)
				PORTCbits.RC2 = 1;

			PORTEbits.RE2 = 1;
			// printf_tiny("A%x B%x C%x D%x E%x\n", PORTA, PORTB, PORTC, PORTD, PORTE);
			TMR0H = 0;
			TMR0L = 0;
			state = 1;
			lamp = 1;
		} else if (istate) {
			state = 10;
		} else if (j != lamp) {
			lamp = j;
			printf_tiny("L%d ", lamp);
			txBuffer[txbp++] = 128 + lamp;
		} else if (x > dl3) {
			if (lamp)
				printf_tiny("_");
			else
				printf_tiny("#");
			txBuffer[txbp++] = 128 + lamp;
			TMR0H = 0;
			TMR0L = 0;
		}
		break;
	case 1:
		/*
		 * Keypress, release data bits
		 */
		if (x < dl2) 
			break;
		PORTAbits.RA0 = 0;
		PORTAbits.RA1 = 0;
		PORTAbits.RA2 = 0;
		PORTAbits.RA3 = 0;
		PORTCbits.RC0 = 0;
		PORTCbits.RC1 = 0;
		PORTCbits.RC2 = 0;
		PORTAbits.RA0 = 0;
		state = 2;
		break;
	case 2:
		/*
		 * Keypress, release ready
		 */
		if (x < dl1) 
			break;
		PORTEbits.RE2 = 0;
		state = 0;
		break;
	case 10:
		/*
		 * Print, sample data
		 */
		if (!PORTBbits.RB1)
			break;
		TMR0H = 0;
		TMR0L = 0;
		state = 11;
		break;
	case 11:
		if (x < dl4) 
			break;
		PORTBbits.RB0 = 0;
		istate = 0;
		INTCON3bits.INT1IF = 0;
		INTCON3bits.INT1IE = 1;
		j = ((~PORTD) >> 1) & 0x7f;
		txBuffer[txbp++] = j;
		printf_tiny("G:%x ", j);
		state = 0;
		break;
	default:
		printf_tiny("Illegal state %d\n", state);
		state = 0;
		break;
	}
	Send();
}

#endif

/*********************************************************************/

void
intr_h() __interrupt(1)
{

#if 1
	if (INTCON3bits.INT1IF) {
		if(PORTBbits.RB1) {
			INTCON3bits.INT1IF = 0;
		} else {
			istate = 1;
			PORTBbits.RB0 = 1;
			INTCON3bits.INT1IF = 0;
			INTCON3bits.INT1IE = 0;
			putchar('^' + PORTBbits.RB1);
		}
	}
#endif
#if USB
	if (PIR2bits.USBIF) {
		USB_intr();
		PIR2bits.USBIF = 0;
	}
#endif
#if SERIAL
	if (PIR1bits.TXIF) {
		PIE1bits.TXIE = 0;
		pc_poll();
	}
#endif
}

void
intr_l() __interrupt(2)
{
	intr_h();
}

/*********************************************************************/

void
main(void) __wparam 
{

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
	 * T0 Freq = 48MHz / (4 * 32) = 350 kHz
	 */
	T0CON = 0
	    | (1 << 7)		// Enable
	    | (4 << 0)		// 1:16 Prescaler
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

	INTCON2bits.RBPU = 1;		// Weak pull-up PORTB
	INTCON2bits.INTEDG1 = 0;	// INT1 on falling edge

	PORTEbits.RDPU = 1;
	/* Setup Interrupts */

	{
	uint8_t u, v;
	u = RCON;
	v = STKPTR;
	printf_tiny("RCON %x %x\r\n", u, v);
	printf_tiny("ADCON1 %x\r\n", ADCON1);
	STKPTRbits.STKFUL = 0;
	STKPTRbits.STKUNF = 0;

	RCON=3;
	}

	RCONbits.IPEN = 1;
	INTCONbits.GIEH = 1;

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


	/*************************************************************/
	PORTAbits.RA0 = 0; TRISAbits.TRISA0 = 0;
	PORTAbits.RA1 = 0; TRISAbits.TRISA1 = 0;
	PORTAbits.RA2 = 0; TRISAbits.TRISA2 = 0;
	PORTAbits.RA3 = 0; TRISAbits.TRISA3 = 0;

	PORTBbits.RB0 = 0; TRISBbits.TRISB0 = 0;

	PORTCbits.RC0 = 0; TRISCbits.TRISC0 = 0;
	PORTCbits.RC1 = 0; TRISCbits.TRISC1 = 0;
	PORTCbits.RC2 = 0; TRISCbits.TRISC2 = 0;
	PORTEbits.RE2 = 0; TRISEbits.TRISE2 = 0;

	INTCON3bits.INT1IP = 1;
	INTCON3bits.INT1IF = 0;
	INTCON3bits.INT1IE = 1;

	ADCON1 = 0xf;

	/*************************************************************/
	PIE1 = 0;
	PIE2 = 0;
	PIE2bits.USBIE = 1;

	state = 0;
	txbp = 0;
	rxbp = 0;
	rxbe = 0;
	lamp = 2;

	while(1) {
		ClrWdt();
		EnableUSBModule();
		USBEcho();
	}
}
