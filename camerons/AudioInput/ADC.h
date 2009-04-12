/*
 * Drive the ADC via the SPI port.
 *
 * Peter Gammie.
 * peteg42 at gmail dot com
 *
 * Commenced September 2008.
 */

#ifndef __ADC_H__
#define __ADC_H__

#include <avr/io.h>

// ADC-specific bits
// The control word is actually 12 bits long. The least significant 4
// bits are ignored. It needs to be sent MSB-first.
// AD7928 data sheet rev B, p15 and p25.

// write to the ADC's control register
#define ADC_WRITE (1 << 15)
// use the sequencer?
#define ADC_SEQ (1 << 14)
// The input number, from 0-7.
#define ADC_ADDR(x) ((x & 0x07) << 10)
// normal power operation: full steam ahead.
#define ADC_PM_NORMAL (0x03 << 8)
// FIXME access the shadow register, used in concert with SEQ
#define ADC_SHADOW (1 << 7)
// Range: Set: analog input 0-2xRefIN. Unset: 0-RefIN.
#define ADC_RANGE_REFIN (0 << 5)
#define ADC_RANGE_2XREFIN (1 << 5)
// Coding: Unset: two's-complement. Set: straight binary.
#define ADC_CODING_TWOS_COMPLEMENT (0 << 4)
#define ADC_CODING_STRAIGHT_BINARY (1 << 4)

/* The basic ADC control register. OR'ed with the address bits. */
#define ADC_CR_VAL (ADC_WRITE | ADC_PM_NORMAL | ADC_RANGE_2XREFIN | ADC_CODING_TWOS_COMPLEMENT)

/* Name the SPI-specific registers/bits - see avr-lib's io.h for the basic definitions. */
#define DD_SS       PINB0
#define DD_SCK      PINB1
#define DD_MOSI     PINB2
#define DD_MISO     PINB3
#define DD_OC1A		PINB5

/* Internal use only: Send a byte through the SPI interface, blocking
 * until the transfer is complete.
 * Note this does not fiddle with the SS-bar (chip select) line. */
static inline uint8_t SPI_SendByte(const uint8_t Byte)
{
	SPDR = Byte;
	while (!(SPSR & (1 << SPIF)));
	return SPDR;
}

/* The ADC transactions are 16 bits long. This takes care of the
 * SS-bar (chip select) line.
 */
static inline uint16_t SPI_SendWord(const uint16_t Word)
{
	uint16_t rxData;

	// Reset SS (chip select). Active low.
	PORTB &= ~(1 << DD_SS);

	// send MS byte of given data
	rxData = (SPI_SendByte((Word >> 8) & 0x00FF)) << 8;
	// send LS byte of given data
	rxData |= (SPI_SendByte(Word & 0x00FF));

	// Set SS (chip select)
	PORTB |= (1 << DD_SS);

	// return the received data
	return rxData;
}

static inline int16_t ADC_ReadSampleAndSetNextAddr(const uint8_t addr)
{
	/* Drop the leading 0 and the three address bits, retaining the 12-bit sample. */
	/* FIXME verify: the USB spec wants the significant bits left-aligned, formats doc 2.2.2 p9. */
	return ((int16_t)SPI_SendWord(ADC_CR_VAL | ADC_ADDR(addr))) << 4;
}

/* Initialise the SPI interface and the ADC. */
static inline void ADC_Init(void)
{
	/* Turn the SPI module on. */
	PRR0 &= ~(1 << PRSPI);

	// Set the SS/SCK/MOSI pins as outputs. Note MISO must be an input.
	DDRB |= (1 << DD_SS) | (1 << DD_SCK) | (1 << DD_MOSI);
	
	// FIXME debug turn on OC1A output pin so we can see it on the CRO
	TCCR1A |= (1 << COM1A0);
	DDRB |= (1 << DD_OC1A);

	// set the slave select high (off), and set slave input high (MyUSB & AVR sample code do this)
	// AVR sample code also sets the clock and output pins high, so I'm doing that too.
	PORTB |= (1 << DD_SS) | (1 << DD_SCK) | (1 << DD_MOSI) | (1 << DD_MISO);

	/* Configure the SPI component: enable, master, clk/2 (bits 1,0 == 00)
	 * SPIE: enable SPI interrupts
	 * CPOL: clock rests high (leading edge falling)
	 * CPHA: 0, leading edge RX (Sample), trailing edge TX (Setup).
	 * DORD: 0, MSB first.
	 */
	SPCR = ((1 << SPE) | (1 << MSTR) | (1 << CPOL));
	SPSR = (1 << SPI2X);

	/* Perform a dummy conversion. p17, Fig 11 of the datasheet. */
	SPI_SendWord(0xFFFF);
}

#endif /* __ADC_H__ */
