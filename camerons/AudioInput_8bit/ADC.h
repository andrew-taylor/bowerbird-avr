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

/* ADC-specific bits
 * The control word is actually 12 bits long. The least significant 4
 * bits are ignored. It needs to be sent MSB-first.
 * AD7928 data sheet rev B, p15 and p25.
 */

// write to the ADC's control register (MSB)
#define ADC_WRITE (1 << 7)
/* use the sequencer? */
#define ADC_SEQ (1 << 6)
/* The input number, from 0-7. */
#define ADC_ADDR_MASK (0x07)
#define ADC_ADDR(x) ((x & ADC_ADDR_MASK) << 2)
/* normal power operation: full steam ahead. */
#define ADC_PM_NORMAL (0x03)

/* write to the ADC's control register (LSB) */
/* FIXME access the shadow register, used in concert with SEQ */
#define ADC_SHADOW (1 << 7)
/* Range: Set: analog input 0-2xRefIN. Unset: 0-RefIN. */
#define ADC_RANGE_REFIN (0 << 5)
#define ADC_RANGE_2XREFIN (1 << 5)
/* Coding: Unset: two's-complement. Set: straight binary.*/
#define ADC_CODING_TWOS_COMPLEMENT (0 << 4)
#define ADC_CODING_STRAIGHT_BINARY (1 << 4)

/* The basic ADC control register, including address bits. */
#define ADC_CR_MSB (ADC_WRITE | ADC_PM_NORMAL)
#define ADC_CR_LSB (ADC_RANGE_2XREFIN | ADC_CODING_TWOS_COMPLEMENT)
#define ADC_CR_ADDR(x) (((ADC_CR_MSB | ADC_ADDR(x)) << 8) | ADC_CR_LSB)


#ifndef __ASSEMBLER__

#include <stdint.h>

/* Initialise the SPI interface and the ADC. */
void ADC_Init(void);

// reimplemented in assembly
int16_t ADC_ReadSampleAndSetNextAddr(const uint8_t address);

#endif /* __ASSEMBLER__ */

#endif /* __ADC_H__ */
