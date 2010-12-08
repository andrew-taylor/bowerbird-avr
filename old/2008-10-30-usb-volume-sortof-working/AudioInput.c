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
/* #include "ADC.h" */
/* #include "PreAmps.h" */

/* Scheduler Task List */
TASK_LIST
{
	{ Task: USB_USBTask          , TaskStatus: TASK_STOP },
	{ Task: USB_Audio_Task       , TaskStatus: TASK_STOP },
};

/* The basic ADC control register. OR'ed with the address bits. */
#define ADC_CR_VAL (ADC_WRITE | ADC_PM_NORMAL | ADC_RANGE_2XREFIN | ADC_CODING_TWOS_COMPLEMENT)

int main(void)
{
  /* Disable watchdog if enabled by bootloader/fuses */
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  /* Disable Clock Division */
  SetSystemClockPrescaler(0);

  /* Initialise the ADC and PreAmps. */
/*   ADC_Init(); */
/*   PreAmps_Init(); */

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
  // FIXME verify: I think this is just an 8-bit counter.
  // So for an 8MHz clock AUDIO_SAMPLE_FREQUENCY must be > 31kHz.
  // There is a 16-bit one, and we probably want the sampling to be interrupt-driven.
/*   OCR0A   = ( (unsigned int)F_CPU / AUDIO_SAMPLE_FREQUENCY); */
/*   TCCR0A  = (1 << WGM01);  // Clear-timer-on-compare-match (CTC) mode */
/*   TCCR0B  = (1 << CS00);   // Fcpu speed */
}

EVENT_HANDLER(USB_Disconnect)
{
  /* Stop the sample reload timer */
/*   TCCR0B = 0; */

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

// FIXME just for debugging.
static uint16_t gain = 101;

EVENT_HANDLER(USB_UnhandledControlPacket)
{
  if((bmRequestType & CONTROL_REQTYPE_TYPE) == REQTYPE_CLASS) {

    /* Audio-class specific. */
    switch(bmRequestType) {

    case AUDIO_REQ_TYPE_Get:
      switch(bRequest) {

      case AUDIO_REQ_GET_Cur:
      case AUDIO_REQ_GET_Min:
      case AUDIO_REQ_GET_Max:
      case AUDIO_REQ_GET_Res:
	{
	  uint16_t wValue  = Endpoint_Read_Word_LE();
	  uint16_t wIndex  = Endpoint_Read_Word_LE();
	  uint16_t wLength = Endpoint_Read_Word_LE();

	  /* A request for the current setting of a particular channel's input gain. */
	  uint8_t ControlSelector = wValue >> 8;
	  uint8_t ChannelNumber   = wValue & 0xFF;

	  // FIXME no master control.
	  // FIXME if ChannelNumber == 0xFF, then get all gain control settings.
	  if(ControlSelector != FEATURE_VOLUME
	     || (ChannelNumber == 0 || ChannelNumber > AUDIO_CHANNELS)) {
	    goto error;
	  }

	  /* Ensure the host is talking about the feature unit. */
	  uint8_t Interface = wIndex & 0xFF;
	  uint8_t EntityID  = wIndex >> 8;

	  if(EntityID != FEATURE_UNIT_ID) {
	    goto error;
	  }

	  // FIXME take care of wLength

	  switch(bRequest) {
	  case AUDIO_REQ_GET_Cur:
	    {
	      // Ask the pre-amp for the current setting. FIXME
	      int16_t gain = 46;

	      //	      gain = PreAmps_get(1);

	      Endpoint_ClearSetupReceived();
	      Endpoint_Write_Control_Stream_LE(&gain, 2); // sizeof(gain));
	      Endpoint_ClearSetupOUT();
	    }	
	    return;

	    // FIXME adjust these in terms of dB's.
	  case AUDIO_REQ_GET_Min:
	    {
	      int16_t min = 0;

	      Endpoint_ClearSetupReceived();
	      Endpoint_Write_Control_Stream_LE(&min, 2); // sizeof(min));
	      Endpoint_ClearSetupOUT();
	    }
	    return;

	  case AUDIO_REQ_GET_Max:
	    {
	      int16_t max = 255;

	      Endpoint_ClearSetupReceived();
	      Endpoint_Write_Control_Stream_LE(&max, 2); // sizeof(max));
	      Endpoint_ClearSetupOUT();
	    }
	    return;

	  case AUDIO_REQ_GET_Res:
	    {
	      int16_t res = 1;

	      Endpoint_ClearSetupReceived();
	      Endpoint_Write_Control_Stream_LE(&res, 2); // sizeof(res));
	      Endpoint_ClearSetupOUT();
	    }
	    return;

	  default:
	    goto error;
	  }
	}
	break;

      default:
	goto error;
      }
      break;

    case AUDIO_REQ_TYPE_Set:
      switch(bRequest) {

      case AUDIO_REQ_SET_Cur:
	{
	  uint16_t wValue  = Endpoint_Read_Word_LE();
	  uint16_t wIndex  = Endpoint_Read_Word_LE();
	  uint16_t wLength = Endpoint_Read_Word_LE();

	  /* A request for the current setting of a particular channel's input gain. */
	  uint8_t ControlSelector = wValue >> 8;
	  uint8_t ChannelNumber   = wValue & 0xFF;

	  // FIXME no master control.
	  // FIXME if ChannelNumber == 0xFF, then get all gain control settings.
	  if(ControlSelector != FEATURE_VOLUME
	     || (ChannelNumber == 0 || ChannelNumber > AUDIO_CHANNELS)) {
	    goto error;
	  }

	  /* Ensure the host is talking about the feature unit. */
	  uint8_t Interface = wIndex & 0xFF;
	  uint8_t EntityID  = wIndex >> 8;

	  if(EntityID != FEATURE_UNIT_ID) {
	    goto error;
	  }

	  // FIXME take care of wLength

	  // Tell the pre-amp for the new setting. FIXME

/* 	      gain = PreAmps_get(0); */

	  Endpoint_ClearSetupReceived();
	  Endpoint_Read_Control_Stream_LE(&gain, sizeof(gain));
	  Endpoint_ClearSetupIN();

	  return;
	}
	break;

      default:
	goto error;
      }
      break;

    default:
      goto error;
    }
  }

 error:
  /* Flush what we've read and tell the host to abort this transaction. */
  Endpoint_ClearSetupReceived();
  Endpoint_StallTransaction();

  return;
}

/* **************************************** */

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
/* 	  if (!(TIFR0 & (1 << OCF0A))) */
/* 	    continue; */
/* 	  else */
/* 	    TIFR0 |= (1 << OCF0A); */

	  /* Output a fairly slow square wave on the left channel. */
  	  int16_t L = val * gain * 64;
/*  	  int16_t L = val * 128 * 64; */

	  if(++slowdown >= 1024) {
	    slowdown = 0;
	    val = -val;
	  }

	  /* Read the microphone board. */
	  int16_t R = 0;

	  /* Tell it we want to read. It returns some garbage. */
/*  	  R = SPI_SendWord(ADC_CR_VAL | ADC_ADDR(0)); */

	  // USB Spec, "Frmts20 final.pdf" p16: left-justify the data.
	  // i.e. for 12 significant bits, shift right 4, have four 0 LSBs.
	  // Note we have to get the sign bit right.
	  //	  R = (R & 0x0FFF);
	  R <<= 4;

	  /* Write the sample to the buffer */
	  Endpoint_Write_Word_LE(L);
	  Endpoint_Write_Word_LE(R);
	}
		
      /* Send the full packet to the host */
      Endpoint_ClearCurrentBank();
    }
}