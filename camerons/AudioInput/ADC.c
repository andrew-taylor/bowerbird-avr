#include "ADC.h"
#include "Shared.h"
#include <MyUSB/Drivers/USB/USB.h>


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
uint16_t ADC_SendWord(const uint16_t Word)
{
	uint16_t rxData;

	// Enable SS (chip select). Active low.
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


/* Initialise the SPI interface and the ADC. */
void ADC_Init(void)
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
	ADC_SendWord(0xFFFF);
}


int16_t ADC_ReadSampleAndSetNextAddr2(const uint8_t addr)
{
	/* Drop the leading 0 and the three address bits, retaining the 12-bit sample. */
	/* FIXME verify: the USB spec wants the significant bits left-aligned, formats doc 2.2.2 p9. */
	return ((int16_t)ADC_SendWord(ADC_CR_ADDR(addr))) << 4;
}

// reimplemented in assembly
int16_t ADC_ReadSampleAndSetNextAddr(const uint8_t address)
{
	int16_t ret_val;

	asm volatile(
// 			"/* invert PORTC so we can observe when this is called */\n\t"
			 "/* save register 16 */\n\t"
			"mov	__tmp_reg__, r16\n\t"
			"/* enable nSS (chip select) DD_SS(0) pin on PORTB */\n\t"
			"cbi	%[portb],		0\n\t"
			"/* load MSB of word to send to ADC */\n\t"
			"mov	r16,		%[address]\n\t"
			"andi	r16,		0x07\n\t"
			"lsl	r16\n\t"
			"lsl	r16\n\t"
			"or		r16,		%[adc_cr_msb]\n\t"
			"/* write the MSB to the SPI data register (SPDR) */\n\t"
			"sts	0x4E,		r16\n\t"
			"/* wait for interrupt flag (SPIF pin in SPSR) to be set (when data is ready) */\n"
		"msloop:\n\t"
			"lds	r16,		0x4D\n\t"
			"sbrs	r16,		7\n\t"
			"rjmp	msloop\n\t"
			"/* save MSB of return value (from SPDR) */\n\t"
			"lds	%B0,		0x4E\n\t"
			"/* shift 4x to the left, discarding overflow (faster to swap & mask) */\n\t"
			"swap	%B0\n\t"
			"andi	%B0,		0xF0\n\t"
			"/* load LSB of word to send to ADC */\n\t"
			"mov	r16,		%[adc_cr_lsb]\n\t"
			"/* write the MSB to the SPI data register (SPDR) */\n\t"
			"sts	0x4E,		r16\n\t"
			"/* wait for interrupt flag (SPIF pin in SPSR) to be set (when data is ready) */\n"
		"lsloop:\n\t"
			"lds	r16,		0x4D\n\t"
			"sbrs	r16,		7\n\t"
			"rjmp	lsloop\n\t"
			"/* save LSB of return value (from SPDR) */\n\t"
			"lds	%A0,		0X4E\n\t"
			"/* disable nSS (chip select) DD_SS(0) pin on PORTB */\n\t"
			"sbi	%[portb],		0\n\t"
			"/* shift 4x to the left, with overflow going into MSB */\n\t"
			"swap	%A0\n\t"
			"mov	r16,		%A0\n\t"
			"andi	%A0,		0xF0\n\t"
			"andi	r16,		0x0F\n\t"
			"or		%B0,		r16\n\t"
			"/* restore register 16 */\n\t"
			"mov	r16,		__tmp_reg__\n\t"
	: "=&r" (ret_val)
	: [address] "r" (address)
			, [adc_cr_msb] "r" (ADC_CR_MSB)
			, [adc_cr_lsb] "r" (ADC_CR_LSB)
			, [portb] "I" (_SFR_IO_ADDR(PORTB))
// 			, [portc] "I" (_SFR_IO_ADDR(PORTC))
// 			, [spdr] "I" (_SFR_MEM_ADDR(SPDR))
// 			, [spsr] "I" (_SFR_MEM_ADDR(SPSR))
// 			, [spif] "I" ((SPIF))
// 			, [nCS] "I" ((DD_SS))
		);
	
	return ret_val;
}
