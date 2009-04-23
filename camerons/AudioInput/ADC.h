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

/* Initialise the SPI interface and the ADC. */
void ADC_Init(void);

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
uint16_t SPI_SendWord(const uint16_t Word);

int16_t ADC_ReadSampleAndSetNextAddr2(const uint8_t addr);

// reimplemented in assembly
int16_t ADC_ReadSampleAndSetNextAddr(const uint8_t address);

#endif /* __ADC_H__ */
