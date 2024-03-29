/* interrupt handler code, which will send/recieve one word to each
 * audio channel via the SPI, and pass the audio data back the usb host
 */

#include "ADC.h"
#include "Shared.h"
#include <avr/io.h>

#define FIFOCON_MASK	(1<<FIFOCON)

__tmp_reg__ = 0
__zero_reg__ = 1

;.global USB_GEN_vect
;USB_GEN_vect:
;		inc		usb_general_interrupt_count
;		reti

.global	TIMER1_COMPA_vect
TIMER1_COMPA_vect:
;		sbi		_SFR_IO_ADDR(PORTC),	0

		/* push our working registers onto the stack */
		push	temp_reg
		/* save status register */
		in		temp_reg,			_SFR_IO_ADDR(SREG)
		push	temp_reg
		
		/* enable nSS (chip select) DD_SS(0) pin on PORTB */
		cbi		_SFR_IO_ADDR(PORTB),	DD_SS
/* write the MSB to the SPI data register (SPDR) */
		sts		SPDR,					write_msb
		/* we now have 16 cycles to wait */
		
		; save register value (1 cycle per push)
		push	usb_ep_save
			
		; save current usb endpoint (2 cycles)
		lds		usb_ep_save,			UENUM

		; select the audio stream endpoint (3 cycles)
		ldi		temp_reg,				AUDIO_STREAM_EPNUM
		sts		UENUM,					temp_reg

		; if audio stream can't be written to, then skip (5 cycles with or without jmp)
		; (this will cause samples to be lost, but there's not much we can do)
		lds		temp_reg,				UEINTX
		sbrs	temp_reg,				RWAL
		rjmp	early_exit_cant_write

		; jump to optimised code if there's only one channel (5 cycles if jumping to mono, else 4)
		tst		multichannel
		brne	non_mono_sample
		rjmp	mono_sample

non_mono_sample:
		; save register value (1 cycle per push)
		push	read_msb

; ADC big byte ready to be read
		/* reading this register seems to be necessary to get the data loaded into SPDR */
		lds		temp_reg,				SPSR
		/* save MSB of return value (from SPDR) */
		lds		read_msb,				SPDR
	
/* write the LSB to the SPI data register (SPDR) */
		sts		SPDR,					write_lsb
		/* we now have 16 cycles to wait */

		; save register value (1 cycle per push)
		push	read_lsb
		push	isr_iter
		push	addr_msb
		push	addr_lsb

		/* initialise loop counter to 1 because we've done one already (1 cycle) */
		ldi		isr_iter,				1
		/* jump to end if there's only one channel
		 * (2 cycles if we're not branching, 3 otherwise) */
		cp		isr_iter,				num_audio_channels
		breq	loop_end

		; get next channel to read after this one
		; load address of element from next_channel array (4 cycles) */		
		ldi		addr_lsb,				lo8(next_channel)
		ldi		addr_msb,				hi8(next_channel)
		adiw	addr_lsb,				1
		; read next channel value from memory address (NOTE - post-incremented) (2 cycles)
		ld		temp_reg,				addr_pair+
		/* set up MSB of word to send to ADC (5 cycles) */
		andi	temp_reg,				ADC_ADDR_MASK
		lsl		temp_reg
		lsl		temp_reg
		ori		temp_reg,				ADC_CR_MSB
		mov		write_msb,				temp_reg

		/* disable nSS (chip select) DD_SS(0) pin on PORTB (2 cycles) */
		sbi		_SFR_IO_ADDR(PORTB),	DD_SS

; ADC little byte ready to be read (2 cycles ago)
		/* reading this register seems to be necessary to get the data loaded into SPDR */
		lds		temp_reg,				SPSR
		/* save LSB of return value (from SPDR) */
		lds		read_lsb,				SPDR

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

		/* rearrange the ADC response to USB audio format (left shift the word 4x, padding with 0s)
		/* shift msb 4x to the left, discarding overflow (faster to swap & mask) (2 cycles) */
		swap	read_msb
		andi	read_msb,				0xF0

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
		; keep track of number of bytes in the usb data buffer (use inc because register < 16)
		inc		bytes_in_usb_buffer
		inc		bytes_in_usb_buffer

		/* Wait for the ADC */
		nop
		;nop
		;nop
		
; ADC big byte ready to be read
		/* reading this register seems to be necessary to get the data loaded into SPDR */
		lds		temp_reg,				SPSR
		/* save MSB of return value (from SPDR) */
		lds		read_msb,				SPDR

/* ADC little byte read/write */
		/* write the LSB to the SPI data register (SPDR) */
		sts		SPDR,					write_lsb
		/* we now have 16 cycles to wait */

		/* increment loop counter (1 cycle) */
		inc		isr_iter
		/* jump to end if there's no more channels
		 * (2 cycles if we're not branching, 3 otherwise) */
		cp		isr_iter,				num_audio_channels
		breq	loop_end

		; get next channel to read after this one
		; read next channel value from memory address (NOTE - post-incremented) (2 cycles)
		ld		temp_reg,				addr_pair+
		/* set up MSB of word to send to ADC (5 cycles) */
		andi	temp_reg,				ADC_ADDR_MASK
		lsl		temp_reg
		lsl		temp_reg
		ori		temp_reg,				ADC_CR_MSB
		mov		write_msb,				temp_reg

		; wait for the ADC (we want the chip de-select to be the last instruction in the loop)
		nop
		nop
		;nop
		;nop
		
		; disable nSS (chip select) DD_SS(0) pin on PORTB (2 cycles)
		sbi		_SFR_IO_ADDR(PORTB),	DD_SS

; ADC little byte ready to be read
		/* reading this register seems to be necessary to get the data loaded into SPDR */
		lds		temp_reg,				SPSR
		/* save MSB of return value (from SPDR) */
		lds		read_lsb,				SPDR
		
		/* loop to top (we break out of the loop elsewhere) */
		rjmp	adcloop

/* We arrive here with the msb read/write of the last sample already done,
 * but the lsb read/write is only 4 cycles in. We still need to read the lsb,
 * and prepare and send this final sample */
loop_end:
		/* rearrange the ADC response to USB audio format (left shift the word 4x, padding with 0s)
		/* shift msb 4x to the left, discarding overflow (faster to swap & mask) (2 cycles) */
		swap	read_msb
		andi	read_msb,				0xF0

		/* setup next first read */
		/** get first channel to read */
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

		/* disable nSS (chip select) DD_SS(0) pin on PORTB */
		sbi		_SFR_IO_ADDR(PORTB),	DD_SS
	
; ADC little byte ready to be read (1 cycle ago)
		/* reading this register seems to be necessary to get the data loaded into SPDR */
		lds		temp_reg,				SPSR
		/* save LSB of return value (from SPDR) */
		lds		read_lsb,				SPDR

		; if this isn't mono, then jump to normal processing
		cpi		isr_iter,				1
		brne	normal_sample
		; for mono sampling, just use the bottom 8 bits
		sts		UEDATX,					read_lsb
		inc		bytes_in_usb_buffer
		rjmp	send_buffer_to_host
		
normal_sample:
		/* process last data collected */
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
		; keep track of number of bytes in the usb data buffer (use inc because register < 16)
		inc		bytes_in_usb_buffer
		inc		bytes_in_usb_buffer
		
send_buffer_to_host:
		/* if buffer is nearly full (assumed to be 256), then send it to the host */
		mov		temp_reg,				num_audio_channels
		lsl		temp_reg
		add		temp_reg,				bytes_in_usb_buffer
		brcc	tidy_up_and_exit
		
		/* send to host (clear FIFOCON bit in UEINTX) */
		lds		temp_reg,				UEINTX
		cbr		temp_reg,				FIFOCON_MASK
		sts		UEINTX,					temp_reg
		clr		bytes_in_usb_buffer

tidy_up_and_exit:
		/* restore registers */
		pop		addr_lsb
		pop		addr_msb
		pop		isr_iter
		pop		read_lsb
		pop		read_msb
early_exit_tidy_up_and_exit:
		/* restore previous endpoint */
		sts		UENUM,					usb_ep_save
		pop		usb_ep_save
		pop		temp_reg
		/* restore status register */
		out		_SFR_IO_ADDR(SREG),		temp_reg
		pop		temp_reg

;		cbi		_SFR_IO_ADDR(PORTC),	0

		/* return from interrupt handler */
		reti

early_exit_cant_write:
		; abort ADC read/write (disable nSS (chip select) DD_SS(0) pin on PORTB)
		sbi		_SFR_IO_ADDR(PORTB),	DD_SS
		; jump to mono exit because we never pushed the extra registers
		rjmp	early_exit_tidy_up_and_exit

; optimised code for fast mono sampling
/* We arrive here with the msb read/write 16 cycles in. We still need to read the lsb,
 * and prepare and send this final sample */
mono_sample:
; ADC big byte ready to be read
		/* reading this register seems to be necessary to get the data loaded into SPDR */
		lds		temp_reg,				SPSR
		/* read MSB of return value (from SPDR), though we'll be discarding this */
		lds		temp_reg,				SPDR
	
/* write the LSB to the SPI data register (SPDR) */
		sts		SPDR,					write_lsb
		/* we now have 16 cycles to wait */

		/* wait for the ADC (nothing else to do here) */
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
		;nop
		;nop
		;nop
		;nop

		/* disable nSS (chip select) DD_SS(0) pin on PORTB (2 cycles) */
		sbi		_SFR_IO_ADDR(PORTB),	DD_SS

; ADC little byte ready to be read
		/* reading this register seems to be necessary to get the data loaded into SPDR */
		lds		temp_reg,				SPSR
		/* save LSB of return value (from SPDR) */
		lds		temp_reg,				SPDR
		; put LSB into USB buffer
		sts		UEDATX,					temp_reg
		; keep track of bytes in the usb data buffer
		inc		bytes_in_usb_buffer

		/* if buffer is full (assumed to be 256), then send it to the host */
		mov		temp_reg,				num_audio_channels
		add		temp_reg,				bytes_in_usb_buffer
		brcc	mono_tidy_up_and_exit
		
		/* send to host (clear FIFOCON bit in UEINTX) */
		lds		temp_reg,				UEINTX
		cbr		temp_reg,				FIFOCON_MASK
		sts		UEINTX,					temp_reg

mono_tidy_up_and_exit:
		/* restore previous endpoint */
		sts		UENUM,					usb_ep_save
		pop		usb_ep_save
		pop		temp_reg
		/* restore status register */
		out		_SFR_IO_ADDR(SREG),		temp_reg
		pop		temp_reg

;		cbi		_SFR_IO_ADDR(PORTC),	0

		/* return from interrupt handler */
		reti
