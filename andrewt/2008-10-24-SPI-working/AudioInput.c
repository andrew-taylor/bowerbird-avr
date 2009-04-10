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

/*
	Audio demonstration application. This gives a simple reference
	application for implementing a USB Audio Input device using the
	basic USB Audio drivers in all modern OSes (i.e. no special drivers
	required).
	
	On startup the system will automatically enumerate and function
	as a USB microphone. Incomming audio from the ADC channel 1 will
	be sampled and sent to the host computer.
	
	To use, connect a microphone to the ADC channel 2.
	
	Under Windows, if a driver request dialogue pops up, select the option
	to automatically install the appropriate drivers.

	      ( Input Terminal )--->---( Output Terminal )
	      (   Microphone   )       (  USB Endpoint   )
*/

/*
	USB Mode:           Device
	USB Class:          Audio Class
	USB Subclass:       Standard Audio Device
	Relevant Standards: USBIF Audio Class Specification
	Usable Speeds:      Full Speed Mode
*/

#include <stdbool.h>

/* I2C/TWI/whatever interface. */
#include <util/twi.h>

#include "AudioInput.h"
#include "SPI.h"

#include <avr/io.h>

/* Scheduler Task List */
TASK_LIST
{
	{ Task: USB_USBTask          , TaskStatus: TASK_STOP },
	{ Task: USB_Audio_Task       , TaskStatus: TASK_STOP },
};

/* Input gain: the first one is the "master" gain. FIXME we ignore this. */
static uint16_t gain[AUDIO_CHANNELS + 1] = {0, 128, 128};

int main(void)
{
  /* Disable watchdog if enabled by bootloader/fuses */
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  /* Disable Clock Division */
  SetSystemClockPrescaler(0);

  /* Hardware Initialization */

  


  /* Initialise the SPI bus: 4MHz clock (for a 8MHz AVR clock). FIXME this is as fast as it goes. */
/*   SPI_Init(SPI_SPEED_FCPU_DIV_2, true); */
//  SPI_Init(SPI_SPEED_FCPU_DIV_16, true);

  /* Initialize Scheduler so that it can be used */
  Scheduler_Init();

  /* Initialize USB Subsystem */
  USB_Init();

  /* Scheduling - routine never returns, so put this last in the main function */
  Scheduler_Start();
}

EVENT_HANDLER(USB_Connect)
{
  /* Start USB management task */
  Scheduler_SetTaskMode(USB_USBTask, TASK_RUN);

  /* Indicate USB enumerating */

  /* Sample reload timer initialization */
  OCR0A   = ( (unsigned int)F_CPU / AUDIO_SAMPLE_FREQUENCY);
  TCCR0A  = (1 << WGM01);  // CTC mode
  TCCR0B  = (1 << CS00);   // Fcpu speed
}

EVENT_HANDLER(USB_Disconnect)
{
  /* Stop the sample reload timer */
  TCCR0B = 0;

  /* Stop running audio and USB management tasks */
  Scheduler_SetTaskMode(USB_Audio_Task, TASK_STOP);
  Scheduler_SetTaskMode(USB_USBTask, TASK_STOP);

  /* Indicate USB not ready */
}

EVENT_HANDLER(USB_ConfigurationChanged)
{
  /* Setup audio stream endpoint */
  Endpoint_ConfigureEndpoint(AUDIO_STREAM_EPNUM, EP_TYPE_ISOCHRONOUS,
			     ENDPOINT_DIR_IN, AUDIO_STREAM_EPSIZE,
			     ENDPOINT_BANK_DOUBLE);

  /* Indicate USB connected and ready */

  /* Start audio task */
  Scheduler_SetTaskMode(USB_Audio_Task, TASK_RUN);
}

/* FIXME control requests. */
enum {
  REQ_TYPE_Get = 0xA1,
  REQ_TYPE_Set = 0x21,

  REQ_CUR = 0x01,
  REQ_RANGE = 0x02
};

EVENT_HANDLER(USB_UnhandledControlPacket)
{
  /* Process General and Audio specific control requests */

  if((bmRequestType & CONTROL_REQTYPE_TYPE) == REQTYPE_CLASS) {
    /* Audio-class specific. */
    switch(bmRequestType) {
    case REQ_TYPE_Get:
      switch(bRequest) {
      case REQ_CUR:
	{
	  /* A request for the current setting of a particular channel's input gain. */
	  /* FIXME this is all quite broken presently. */
	  uint8_t ControlSelector = Endpoint_Read_Byte();
	  uint8_t ChannelNumber   = Endpoint_Read_Byte();

	  if(ControlSelector != FEATURE_INPUT_GAIN_CONTROL || ChannelNumber > AUDIO_CHANNELS) {
	    goto error;
	  }

	  Endpoint_Write_Word_LE(gain[ChannelNumber]);
	  /* Send the full packet to the host */
	  Endpoint_ClearCurrentBank();
	}

	break;

      case REQ_RANGE:
	{
	  /* A request for the range of a particular channel's input gain.
	   * This is the same for all channels. */
	  uint8_t ControlSelector = Endpoint_Read_Byte();
	  uint8_t ChannelNumber   = Endpoint_Read_Byte();

	  if(ControlSelector != FEATURE_INPUT_GAIN_CONTROL || ChannelNumber > AUDIO_CHANNELS) {
	    goto error;
	  }

	  Endpoint_Write_Word_LE(gain[ChannelNumber]);
	  /* Send the full packet to the host */
	  Endpoint_ClearCurrentBank();
	}


	break;
      default:
	goto error;
      }
      break;

      case REQ_TYPE_Set:
      break;
    }
  } else {
    switch(bRequest) {
    case REQ_SetInterface:
      /* Set Interface is not handled by the library, as its function is application-specific */
      if (bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_STANDARD | REQREC_INTERFACE)) {
	Endpoint_ClearSetupReceived();
	Endpoint_ClearSetupIN();
      }
      break;
    }
  }
  return;

 error:
  /* FIXME signal "stall". */
  return;

}

/* **************************************** */

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
// Range: Unset: analog input 0-2xRefIN. Set: 0-RefIN.
#define ADC_RANGE (1 << 5)
// Coding: Unset: two's-complement. Set: straight binary.
#define ADC_CODING_TWOS_COMPLEMENT (0 << 4)
#define ADC_CODING_STRAIGHT_BINARY (1 << 4) 

TASK(USB_Audio_Task)
{
  static int slowdown = 0;
  static int val = 1;

  /* Select the audio stream endpoint */
  Endpoint_SelectEndpoint(AUDIO_STREAM_EPNUM);
	
  /* Check if the current endpoint can be read from (contains a packet) */
  if (Endpoint_ReadWriteAllowed())
    {
      /* Process the endpoint bytes all at once; the audio is at such a high sample rate that this
       * does not have any noticable latency on the USB management task */
      while (Endpoint_BytesInEndpoint() < AUDIO_STREAM_EPSIZE)
	{
	  /* Wait until next audio sample should be processed */
	  if (!(TIFR0 & (1 << OCF0A)))
	    continue;
	  else
	    TIFR0 |= (1 << OCF0A);

	  /* Output a fairly slow square wave on the left channel. */
 	  int16_t L = val * gain[2] * 64;

	  if(++slowdown >= 1024) {
	    slowdown = 0;
	    val = -val;
	  }

	  /* Read the microphone board. */
	  uint16_t R;

  // Do some bit-banging
/*   DDRB = (1 << DD_SS) | (1 << DD_MOSI) | (1 << DD_SCK); */
  DDRB = 0xF7;  
  SPCR = (1<<SPE) | (1<<MSTR) | (1 << CPOL) | (0 << CPHA);
  UCSR1B = (1 << RXEN1) | (1 << TXEN1);

  /* Reset the ADC (do once at boot time). */
  SPI_SendWord(0xFFFF);

/*   while(1) { */
/*     SPI_SendWord(ADC_WRITE | ADC_ADDR(3) | ADC_PM_NORMAL | ADC_CODING_TWOS_COMPLEMENT); */
/* /\*     SPDR = 0x77; *\/ */
/* /\*     while (!(SPSR & (1 << SPIF))); *\/ */
/* /\*     PORTB = 0; *\/ */
/* /\*     PORTB = 0xFF; *\/ */
/*   } */

	  /* Tell it we want to read. It returns some garbage. */
/* 	  R = ADC_WRITE | ADC_ADDR(3) | ADC_PM_NORMAL | ADC_CODING_TWOS_COMPLEMENT; */
	  R = SPI_SendWord(ADC_WRITE | ADC_ADDR(3) | ADC_PM_NORMAL | ADC_CODING_TWOS_COMPLEMENT);

	  /* Now it tells us something interesting. Feed it the same config again. FIXME */
/* 	  R = SPI_SendWord(ADC_WRITE | ADC_ADDR(3) | ADC_PM_NORMAL | ADC_CODING_TWOS_COMPLEMENT); */

	  /* Write the sample to the buffer */
	  Endpoint_Write_Word_LE(L);
	  Endpoint_Write_Word_LE(R);
	}
		
      /* Send the full packet to the host */
      Endpoint_ClearCurrentBank();
    }
}
