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
 * $Id: serial.c,v 1.1 2011/05/19 13:57:37 phk Exp $
 *
 */

#ifndef __SERIAL_C__
#define __SERIAL_C__

#define __RCPIR  (PIR1bits.RCIF)
#define __RCPIR1 (PIR1bits.RC1IF)
#define __RCPIR2 (PIR3bits.RC2IF)

#define __TXPIR	(PIR1bits.TXIF)
#define __TXPIR1 (PIR1bits.TX1IF)
#define __TXPIR2 (PIR3bits.TX2IF)

/* --------------------------------------------------------------------
 */

#define SERIAL_INIT(port)						\
	do {								\
		uint8_t __serial_init_hack;				\
		RCSTA ## port ## bits.CREN = 0;				\
		__serial_init_hack = RCREG ## port;			\
		__serial_init_hack = RCREG ## port;			\
		__serial_init_hack = RCREG ## port;			\
		RCSTA ## port = 0x90;					\
		TXSTA ## port = 0x20;					\
		TXREG ## port = 0x00;					\
	} while (0)


#define SERIAL_BAUD(port, baud) do { SPBRG ## port = baud; } while (0)

#define SERIAL_TXRDY(port) 	(TXSTA ## port ## bits.TRMT)

#define SERIAL_TX(port, byte) 	do { TXREG ## port = byte; } while (0)

#define SERIAL_RXOERR(port)	(RCSTA ## port ## bits.OERR)

#define SERIAL_RXRDY(port) 	(__RCPIR ## port)

#define SERIAL_RX(port) 	(RCREG ## port)

#if 0
#define IF_PORT(m, n)	if ((SERIALPORTS & (1 << n)) && m == n)

static void
serial_init(const uint8_t port)
{
	IF_PORT(port, 0)	SERIAL_INIT();
	// IF_PORT(port, 1)	SERIAL_INIT(1);
	// IF_PORT(port, 2)	SERIAL_INIT(2);
}

static uint8_t
serial_txrdy(const uint8_t port)
{
	IF_PORT(port, 0)	return(__TXPIR);
	// IF_PORT(port, 1)	return(__TXPIR1);
	// IF_PORT(port, 2)	return(__TXPIR2);
	return (0);
}

static void
serial_tx(const uint8_t port, const uint8_t byte)
{
	IF_PORT(port, 0)	SERIAL_TX(, byte);
	// IF_PORT(port, 1)	SERIAL_TX(1, byte);
	// IF_PORT(port, 2)	SERIAL_TX(2, byte);
}

static uint8_t
serial_rxoerr(const uint8_t port)
{
	IF_PORT(port, 0)	return(SERIAL_RXOERR());
	// IF_PORT(port, 1)	return(SERIAL_RXOERR(1));
	// IF_PORT(port, 2)	return(SERIAL_RXOERR(2));
	return (0);
}

static uint8_t
serial_rxrdy(const uint8_t port)
{
	IF_PORT(port, 0)	return(SERIAL_RXRDY());
	// IF_PORT(port, 1)	return(SERIAL_RXRDY(1));
	// IF_PORT(port, 2)	return(SERIAL_RXRDY(2));
	return (0);
}

static uint8_t
serial_rx(const uint8_t port)
{
	IF_PORT(port, 0)	return(SERIAL_RX());
	// IF_PORT(port, 1)	return(SERIAL_RX(1));
	// IF_PORT(port, 2)	return(SERIAL_RX(2));
	return (0);
}

#endif
#endif /* __SERIAL_C__ */
