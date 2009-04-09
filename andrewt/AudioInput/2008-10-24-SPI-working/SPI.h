/*
             MyUSB Library
     Copyright (C) Dean Camera, 2008.
              
  dean [at] fourwalledcubicle [dot] com
      www.fourwalledcubicle.com
*/

/*
  Copyright 2008  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, and distribute this software
  and its documentation for any purpose and without fee is hereby
  granted, provided that the above copyright notice appear in all
  copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/* peteg: change a few things. */

/** \file
 *
 *  SPI subsystem driver for the supported USB AVRs models.
 */

#ifndef __SPI_H__
#define __SPI_H__

/* Includes: */
#include <avr/io.h>
#include <stdbool.h>

/* Name the SPI-specific registers/bits - see avr-lib's io.h for the basic definitions.
 * Follows AVR151 app sheet. */
#define DD_SCK      PINB7
#define DD_MOSI     PINB5
#define DD_SS       PINB4

/* Private Interface - For use in library only: */
#if !defined(__DOXYGEN__)
/* Macros: */
#define SPI_USE_DOUBLESPEED            (1 << 7)
#endif
	
/* Public Interface - May be used in end-application: */
/* Macros: */
/** SPI prescaler mask for SPI_Init(). Divides the system clock by a factor of 2. */
#define SPI_SPEED_FCPU_DIV_2           SPI_USE_DOUBLESPEED

/** SPI prescaler mask for SPI_Init(). Divides the system clock by a factor of 4. */
#define SPI_SPEED_FCPU_DIV_4           0

/** SPI prescaler mask for SPI_Init(). Divides the system clock by a factor of 8. */
#define SPI_SPEED_FCPU_DIV_8           (SPI_USE_DOUBLESPEED | (1 << SPR0))

/** SPI prescaler mask for SPI_Init(). Divides the system clock by a factor of 16. */
#define SPI_SPEED_FCPU_DIV_16          (1 << SPR0)

/** SPI prescaler mask for SPI_Init(). Divides the system clock by a factor of 32. */
#define SPI_SPEED_FCPU_DIV_32          (SPI_USE_DOUBLESPEED | (1 << SPR1))

/** SPI prescaler mask for SPI_Init(). Divides the system clock by a factor of 64. */
#define SPI_SPEED_FCPU_DIV_64          (SPI_USE_DOUBLESPEED | (1 << SPR1) | (1 < SPR0))

/** SPI prescaler mask for SPI_Init(). Divides the system clock by a factor of 128. */
#define SPI_SPEED_FCPU_DIV_128         ((1 << SPR1) | (1 < SPR0))

/* Inline Functions: */
/** Initializes the SPI subsystem, ready for transfers. Must be called before calling any other
 *  SPI routines.
 *
 *  \param PrescalerMask  Prescaler mask to set the SPI clock speed
 *  \param Master         If true, sets the SPI system to use master mode, slave if false
 */
static inline void SPI_Init(const uint8_t PrescalerMask, const bool Master)
{
#define SPI_CLK_PIN_PORT	PORTD
#define SPI_CLK_PIN_DDR	DDRD
#define SPI_CLK_PIN		PIND5
#define SPI_SS_PIN		PIND4

	UBRR1 = 0; /* apparently this needs to be 0 during setup */
	UCSR1B = (1<<RXEN1) | (1<<TXEN1);
	UCSR1C = (1<<UMSEL11) | (1<<UMSEL10);
	/* Calculated value from appnote code. ((CPUFREQ/(2*BPS))-1) */
	UBRR1 = ((F_CPU/(20000))-1);
	SPI_CLK_PIN_DDR  |=  _BV(SPI_CLK_PIN);

/*         DDRB    = (1<<PB4)|(1<<PB5)|(1<<PB7); */
/*         // enable SPI in Master Mode with SCK = CK/4 */
/*         SPCR    = (1<<SPE)|(1<<MSTR); */

/*         volatile char IOReg; */
/*         IOReg   = SPSR;                 	// clear SPIF bit in SPSR */
/*         IOReg   = SPDR; */

/*   // set MOSI and SCK as output pins, all others input. */
/*   DDRB = (1 << DD_SS) | (1 << DD_MOSI) | (1 << DD_SCK); */

/*   // From the ADC data sheet: */
/*   // CPOL: 1 - clock rests high */
/*   // CPHA: 0 - shift (output) on the rising edge. */

/*   // From the AVR data sheet: */
/*   // MSTR: AVR is the master on the SPI bus */
/*   // SPE: enable SPI mode. */
/*   // FIXME SCK = CK/4 */
/*   SPCR = (1 << SPE) | ((!!Master) << MSTR) | (1 << CPOL) | (0 << CPHA); */

/*   // FIXME turn the TX on, AVR data sheet p210 */
/*   UCSR1B = (1 << RXEN1) | (1 << TXEN1); */


  // FIXME is this voodoo necessary?
  volatile uint8_t IOReg;
  IOReg   = SPSR;                 	// clear SPIF bit in SPSR
  IOReg   = SPDR;

/* /\*   DDRB  |= ((1 << 1) | (1 << 2)); *\/ */
/* /\*   DDR_SPI |= ((1 << 0) | (1 << 3)); *\/ */

/* /\*   SPCR   = ((1 << SPE) | (Master << MSTR) | (1 << CPOL) | (1 << CPHA) *\/ */
/* /\* 	    | (PrescalerMask & ~SPI_USE_DOUBLESPEED)); *\/ */

/* /\*   if (PrescalerMask & SPI_USE_DOUBLESPEED) *\/ */
/* /\*     SPSR = (1 << SPI2X); *\/ */
}

/** Sends a byte through the SPI interface, blocking until the transfer is complete.
 *
 *  \param Byte  Byte to send through the SPI interface
 */
static inline uint8_t SPI_SendByte(const uint8_t Byte)
{
  SPDR = Byte;
  while (!(SPSR & (1 << SPIF)));
  return SPDR;
}

/* FIXME */
static inline uint16_t SPI_SendWord(const uint16_t Word)
{
  uint16_t rxData;

  // Reset SS (chip select). Active low.
  PORTB &= ~1;

  // send MS byte of given data
  rxData = (SPI_SendByte((Word >> 8) & 0x00FF)) << 8;
  // send LS byte of given data
  rxData |= (SPI_SendByte(Word & 0x00FF));

  // Set SS (chip select)
  PORTB |= 1;

  // return the received data
  return rxData;
}

#endif
