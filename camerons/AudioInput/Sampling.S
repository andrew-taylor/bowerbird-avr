/* interrupt handler code, which will send/recieve one word to each
 * audio channel via the SPI, and pass the audio data back the usb host
 */

#include "ADC.h"
#include "Shared.h"
#include <avr/io.h>

#define FIFOCON_MASK	(1<<FIFOCON)

__tmp_reg__ = 0
__zero_reg__ = 1

.global	TIMER1_COMPA_vect
TIMER1_COMPA_vect:
		/* push our working registers onto the stack */
		push	__zero_reg__
		push	__tmp_reg__
		/* save status register */
		in		__tmp_reg__,			_SFR_IO_ADDR(SREG)
		push	__tmp_reg__
		clr		__zero_reg__
		push	usb_ep_save
		push	pair_lsb
		push	pair_msb
		push	isr_iter
		push	temp_reg
		push	num_chans
		push	next_chan

		/* save current usb endpoint (anded with mask to guarantee safe) */
		lds		usb_ep_save,			UENUM
		andi	usb_ep_save,			ENDPOINT_EPNUM_MASK

		/* select the audio stream endpoint */
		ldi		temp_reg,				AUDIO_STREAM_EPNUM
		sts		UENUM,					temp_reg

		/** if audio stream can't be written to, then skip
		 * (this will cause samples to be lost, but there's not much we can do) */
		lds		temp_reg,				UEINTX
		sbrs	temp_reg,				RWAL
		rjmp	end

		/* FIXME load number of channels in this configuration from C variable num_audio_channels */
		ldi		pair_lsb,				lo8(num_audio_channels)
		ldi		pair_msb,				hi8(num_audio_channels)
		ld		num_chans,				pair
		
		/* initialise loop */
		clr		isr_iter

adcloop:
		/** get next channel to read after this one (excluding muted)
		 * if channel is muted then send zeroes */
		/* load address of element from next_channel array */
		mov		pair_lsb,				isr_iter
		clr		pair_msb
		subi	pair_lsb,				lo8(-(next_channel))
		sbci	pair_msb,				hi8(-(next_channel))
		/* read next channel value from memory address (-1 == mute) */
		ld		next_chan,				pair
		cpi		next_chan,				-1
		brne	normal_sample
		/* if muted, then load zeroes and skip sampling */
		clr		pair_lsb
		clr		pair_msb
		rjmp	send_data

normal_sample:
		/* next_chan is the adc index of the next channel to read */
/* start of ADC_ReadSampleAndSetNextAddr code */
		/* enable nSS (chip select) DD_SS(0) pin on PORTB */
		cbi		_SFR_IO_ADDR(PORTB),	DD_SS
		/* load MSB of word to send to ADC */
		mov		temp_reg,				next_chan
		andi	temp_reg,				ADC_ADDR_MASK
		lsl		temp_reg
		lsl		temp_reg
		ori		temp_reg,				ADC_CR_MSB
		/* write the MSB to the SPI data register (SPDR) */
		sts		SPDR,					temp_reg
/* wait for interrupt (SPIF pin in SPSR) to be set (data is ready) */
msloop:
		lds		temp_reg,				SPSR
		sbrs	temp_reg,				SPIF
		rjmp	msloop
		/* save MSB of return value (from SPDR) */
		lds		pair_msb,				SPDR
		/* shift 4x to the left, discarding overflow (faster to swap & mask) */
		swap	pair_msb
		andi	pair_msb,				0xF0
		/* load LSB of word to send to ADC */
		ldi		temp_reg,				ADC_CR_LSB
		/* write the MSB to the SPI data register (SPDR) */
		sts		SPDR,					temp_reg
/* wait for interrupt (SPIF pin in SPSR) to be set (data is ready) */
lsloop:
		lds		temp_reg,				SPSR
		sbrs	temp_reg,				SPIF
		rjmp	lsloop
		/* save LSB of return value (from SPDR) */
		lds		pair_lsb,				SPDR
		/* disable nSS (chip select) DD_SS(0) pin on PORTB */
		sbi		_SFR_IO_ADDR(PORTB),	DD_SS
		/* shift 4x to the left, with overflow going into MSB */
		swap	pair_lsb
		mov		temp_reg,				pair_lsb
		andi	pair_lsb,				0xF0
		andi	temp_reg,				0x0F
		or		pair_msb,				temp_reg
/* end of ADC_ReadSampleAndSetNextAddr code */

send_data:
		/* send data to usb (8bit FIFO, so just write twice) */
		sts		UEDATX,					pair_lsb
		sts		UEDATX,					pair_msb

		/* increment counter */
		inc		isr_iter
		/* loop back while still channels to process */
		cp		isr_iter,				num_chans
		brne	adcloop
/* end of adcloop */

		/* if buffer is just about full, then send it to the host */
		lds		pair_lsb,				UEBCLX
		lds		pair_msb,				UEBCHX
		cpi		pair_lsb,				AUDIO_STREAM_FULL_THRESHOLD
		cpc		pair_msb,				__zero_reg__
		brlo	end
		
		/* send to host (clear FIFOCON bit in UEINTX) */
		lds		temp_reg,				UEINTX
		cbr		temp_reg,				FIFOCON_MASK
		sts		UEINTX,					temp_reg

end:
		/* restore previous endpoint */
		sts		UENUM,					usb_ep_save

		/* restore registers */
		pop		next_chan
		pop		num_chans
		pop		temp_reg
		pop		isr_iter
		pop		pair_msb
		pop		pair_lsb
		pop		usb_ep_save
		pop		__tmp_reg__
		/* restore status register */
		out		_SFR_IO_ADDR(SREG),		__tmp_reg__
		pop		__tmp_reg__
		pop		__zero_reg__

		/* return from interrupt handler */
		reti
