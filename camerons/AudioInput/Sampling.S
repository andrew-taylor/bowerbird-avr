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
		push	isr_iter
		push	temp_reg
		push	num_chans
		push	next_chan
		push	read_lsb
		push	read_msb
		push	addr_lsb
		push	addr_msb

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
		rjmp	tidy_up_and_exit

		/* enable nSS (chip select) DD_SS(0) pin on PORTB */
		cbi		_SFR_IO_ADDR(PORTB),	DD_SS
		/* write the MSB to the SPI data register (SPDR) */
		sts		SPDR,					write_msb
		/* we now have 16 cycles to wait */
				 
		/* load number of channels in this configuration from C variable num_audio_channels (4 cycles) */
		ldi		addr_lsb,				lo8(num_audio_channels)
		ldi		addr_msb,				hi8(num_audio_channels)
		ld		num_chans,				addr_pair
				
		/* wait for the ADC (12 cycles) */
 		nop
 		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop

		/* reading this register seems to be necessary to get the data loaded into SPDR */
		lds		temp_reg,				SPSR
		/* save MSB of return value (from SPDR) */
		lds		read_msb,				SPDR
	
		/* write the LSB to the SPI data register (SPDR) */
		sts		SPDR,					write_lsb
		/* we now have 16 cycles to wait */

		/* rearrange the ADC response to USB audio format (left shift the word 4x, padding with 0s)
		/* shift msb 4x to the left, discarding overflow (faster to swap & mask) (2 cycles) */
		swap	read_msb
		andi	read_msb,				0xF0

		/* initialise loop counter to 1 because we've done one already (1 cycle) */
		ldi		isr_iter,				1
		/* jump to end if there's only one channel
		 * (2 cycles if we're not branching, 3 otherwise) */
		cp		isr_iter,				num_chans
		breq	loop_end

		/** get next channel to read after this one (excluding muted)
		 * if channel is muted then send zeroes */
		/* load address of element from next_channel array (4 cycles) */
		mov		addr_lsb,				isr_iter
		clr		addr_msb
		subi	addr_lsb,				lo8(-(next_channel))
		sbci	addr_msb,				hi8(-(next_channel))
		/* read next channel value from memory address (-1 == mute) (2 cycles) */
		ld		temp_reg,				addr_pair
		/* set up MSB of word to send to ADC (5 cycles) */
		andi	temp_reg,				ADC_ADDR_MASK
		lsl		temp_reg
		lsl		temp_reg
		ori		temp_reg,				ADC_CR_MSB
		mov		write_msb,				temp_reg

		/* wait for the ADC */
		/* (not required at present because we have 16 cycles of instructions) */

		/* reading this register seems to be necessary to get the data loaded into SPDR */
		lds		temp_reg,				SPSR
		/* save LSB of return value (from SPDR) */
		lds		read_lsb,				SPDR
		
		/* disable nSS (chip select) DD_SS(0) pin on PORTB */
		sbi		_SFR_IO_ADDR(PORTB),	DD_SS

/** at the beginning of each of these loops write_msb/lsb are setup with the
 * word to send to the ADC, and read_msb/lsb contain the previous word read back
 * (though msb has already been left shifted x4) */
adcloop:
		/* enable nSS (chip select) DD_SS(0) pin on PORTB */
		cbi		_SFR_IO_ADDR(PORTB),	DD_SS
/* ADC big byte read/write */
		/* write the MSB to the SPI data register (SPDR) */
		sts		SPDR,					write_msb
		/* we now have 16 cycles to wait */

		/* rearrange the previous ADC response to USB audio format
		 * (flushed left, padding with 0s)
		 * shift 4x to the left, with overflow going into MSB (5 cycles) */
		swap	read_lsb
		mov		temp_reg,				read_lsb
		andi	read_lsb,				0xF0
		andi	temp_reg,				0x0F
		or		read_msb,				temp_reg
		
		/* send previous data to usb (8bit FIFO, so just write twice) (4 cycles) */
		sts		UEDATX,					read_lsb
		sts		UEDATX,					read_msb

		/* Wait for the ADC */
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		
		/* reading this register seems to be necessary to get the data loaded into SPDR */
		lds		temp_reg,				SPSR
		/* save MSB of return value (from SPDR) */
		lds		read_msb,				SPDR

/* ADC little byte read/write */
		/* write the LSB to the SPI data register (SPDR) */
		sts		SPDR,					write_lsb
		/* we now have 16 cycles to wait */

		/* rearrange the ADC response to USB audio format (left shift the word 4x, padding with 0s)
		/* shift msb 4x to the left, discarding overflow (faster to swap & mask) (2 cycles) */
		swap	read_msb
		andi	read_msb,				0xF0

		/* increment loop counter (1 cycle) */
		inc		isr_iter
		/* jump to end if there's no more channels
		 * (2 cycles if we're not branching, 3 otherwise) */
		cp		isr_iter,				num_chans
		breq	loop_end

		/** get next channel to read after this one (excluding muted)
		 * if channel is muted then send zeroes */
		/* load address of element from next_channel array (4 cycles) */
		mov		addr_lsb,				isr_iter
		clr		addr_msb
		subi	addr_lsb,				lo8(-(next_channel))
		sbci	addr_msb,				hi8(-(next_channel))
		/* read next channel value from memory address (-1 == mute) (2 cycles) */
		ld		temp_reg,				addr_pair
		/* set up MSB of word to send to ADC (5 cycles) */
		andi	temp_reg,				ADC_ADDR_MASK
		lsl		temp_reg
		lsl		temp_reg
		ori		temp_reg,				ADC_CR_MSB
		mov		write_msb,				temp_reg

		/* wait for the ADC */
		/* (not required at present because we have 16 cycles of instructions) */

		/* reading this register seems to be necessary to get the data loaded into SPDR */
		lds		temp_reg,				SPSR
		/* save MSB of return value (from SPDR) */
		lds		read_lsb,				SPDR
		
		/* disable nSS (chip select) DD_SS(0) pin on PORTB */
		sbi		_SFR_IO_ADDR(PORTB),	DD_SS

		/* loop to top (we break out of the loop elsewhere) */
		rjmp	adcloop

/* We arrive here with the msb read/write of the last sample already done,
 * but the lsb read/write is only 6 cycles in. We still need to read the lsb,
 * and prepare and send this final sample */
loop_end:
		/* setup next first read */
		/** get next channel to read after this one */
		/* load address of element from next_channel array (2 cycles)*/
		ldi		addr_lsb,				lo8(next_channel)
		ldi		addr_msb,				hi8(next_channel)
		/* read next channel value from memory address (-1 == mute) (2 cycles)*/
		ld		temp_reg,				addr_pair
		/* load MSB of word to send to ADC (5 cycles) */
		andi	temp_reg,				ADC_ADDR_MASK
		lsl		temp_reg
		lsl		temp_reg
		ori		temp_reg,				ADC_CR_MSB
		mov		write_msb,				temp_reg

		/* process last data collected */
		/* wait the remaining cycle */
		nop
	
		/* reading this register seems to be necessary to get the data loaded into SPDR */
		lds		temp_reg,				SPSR
		/* save MSB of return value (from SPDR) */
		lds		read_lsb,				SPDR
		
		/* disable nSS (chip select) DD_SS(0) pin on PORTB */
		sbi		_SFR_IO_ADDR(PORTB),	DD_SS
		
		/* rearrange the ADC response to USB audio format (left shift the word 4x, padding with 0s)
		/* shift 4x to the left, with overflow going into MSB */
		swap	read_lsb
		mov		temp_reg,				read_lsb
		andi	read_lsb,				0xF0
		andi	temp_reg,				0x0F
		or		read_msb,				temp_reg
		
		/* send data to usb (8bit FIFO, so just write twice) */
		sts		UEDATX,					read_lsb
		sts		UEDATX,					read_msb
		
send_buffer_to_host:
		/* if buffer is just about full, then send it to the host */
		lds		addr_lsb,				UEBCLX
		lds		addr_msb,				UEBCHX
		cpi		addr_lsb,				AUDIO_STREAM_FULL_THRESHOLD
		cpc		addr_msb,				__zero_reg__
		brlo	tidy_up_and_exit
		
		/* send to host (clear FIFOCON bit in UEINTX) */
		lds		temp_reg,				UEINTX
		cbr		temp_reg,				FIFOCON_MASK
		sts		UEINTX,					temp_reg

tidy_up_and_exit:
		/* restore previous endpoint */
		sts		UENUM,					usb_ep_save

		/* restore registers */
		pop		addr_msb
		pop		addr_lsb
		pop		read_msb
		pop		read_lsb
		pop		next_chan
		pop		num_chans
		pop		temp_reg
		pop		isr_iter
		pop		usb_ep_save
		pop		__tmp_reg__
		/* restore status register */
		out		_SFR_IO_ADDR(SREG),		__tmp_reg__
		pop		__tmp_reg__
		pop		__zero_reg__

		/* return from interrupt handler */
		reti
