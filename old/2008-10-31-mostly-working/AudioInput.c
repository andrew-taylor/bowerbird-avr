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
	USB Mode:           Device
	USB Class:          Audio Class
	USB Subclass:       Standard Audio Device
	Relevant Standards: USBIF Audio Class Specification
	Usable Speeds:      Full Speed Mode
*/

#include <stdbool.h>

#include "AudioInput.h"
#include "ADC.h"
#include "PreAmps.h"

/* Scheduler Task List */
TASK_LIST
{
  { Task: USB_USBTask,    TaskStatus: TASK_STOP },
  { Task: USB_Audio_Task, TaskStatus: TASK_STOP },
};

int main(void)
{
  /* Disable watchdog if enabled by bootloader/fuses */
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  /* Disable Clock Division */
  SetSystemClockPrescaler(0);

  /* Initialise the ADC and PreAmps. */
  ADC_Init();
  PreAmps_Init();

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
  OCR0A   = ( (unsigned int)F_CPU / AUDIO_SAMPLE_FREQUENCY);
  TCCR0A  = (1 << WGM01);  // Clear-timer-on-compare-match (CTC) mode
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
}

// FIXME just for debugging. The last channel is the square wave.
static int16_t gain[AUDIO_CHANNELS] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 101};

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
	      // Ask the pre-amp for the current setting.

/*  	      gain[ChannelNumber - 1] = PreAmps_get(ChannelNumber - 1); */

	      // FIXME the last channel is our square wave.
	      if(ChannelNumber < AUDIO_CHANNELS) {
		//		gain[ChannelNumber - 1] = PreAmps_set(ChannelNumber - 1, 200);
		gain[ChannelNumber - 1] = PreAmps_get(ChannelNumber - 1);
	      }

	      Endpoint_ClearSetupReceived();
	      Endpoint_Write_Control_Stream_LE(&gain[ChannelNumber - 1], sizeof(int16_t));
	      Endpoint_ClearSetupOUT();
	    }
	    return;

	    // FIXME adjust these in terms of dB's.
	    // FIXME also ensure 0-255 is a valid range.
	  case AUDIO_REQ_GET_Min:
	    {
	      int16_t min = 0x0000;

	      Endpoint_ClearSetupReceived();
	      Endpoint_Write_Control_Stream_LE(&min, sizeof(min));
	      Endpoint_ClearSetupOUT();
	    }
	    return;

	  case AUDIO_REQ_GET_Max:
	    {
	      int16_t max = 0x7FFF;

	      Endpoint_ClearSetupReceived();
	      Endpoint_Write_Control_Stream_LE(&max, sizeof(max));
	      Endpoint_ClearSetupOUT();
	    }
	    return;

	  case AUDIO_REQ_GET_Res:
	    {
	      uint16_t res = 1;

	      Endpoint_ClearSetupReceived();
	      Endpoint_Write_Control_Stream_LE(&res, sizeof(res));
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

	  Endpoint_ClearSetupReceived();
	  Endpoint_Read_Control_Stream_LE(&gain[ChannelNumber - 1], sizeof(int16_t));
	  Endpoint_ClearSetupIN();

	  // FIXME Tell *all* the pre-amps for the new setting.
	  for(int i = 0; i < 8; i++) {
	    PreAmps_set(i, gain[ChannelNumber - 1]);
	  }

	  // Tell the pre-amp for the new setting.
/* 	  if(ChannelNumber < AUDIO_CHANNELS) { */
/* 	    PreAmps_set(ChannelNumber - 1, gain[ChannelNumber - 1]); */
/* 	  } */

	  return;
	}

      default:
	goto error;
      }
      break;

    default:
      goto error;
    }
  } else if (bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_STANDARD | REQREC_INTERFACE)) {
    switch (bRequest) {
    case REQ_SetInterface:
      /* Set Interface is not handled by the library, as its function is application-specific */
      {
	uint16_t wValue = Endpoint_Read_Word_LE();

	Endpoint_ClearSetupReceived();

	/* Check if the host is enabling the audio interface (setting AlternateSetting to 1) */
	if(wValue) {
	  Scheduler_SetTaskMode(USB_Audio_Task, TASK_RUN);
	} else {
	  Scheduler_SetTaskMode(USB_Audio_Task, TASK_STOP);
	}

	/* Handshake the request */
	Endpoint_ClearSetupIN();
      }
      break;
    }
  }

 error:

  return;
}

/* **************************************** */

// FIXME This task gets run continuously, whenever we're
// idle. Hopefully it will be invoked quickly enough.
// Well, it's not going to be real-time. Best if we do it via a timer
// interrupt.

TASK(USB_Audio_Task)
{
  static int slowdown = 0;
  static int val = 1;

  /* Select the audio stream endpoint */
  Endpoint_SelectEndpoint(AUDIO_STREAM_EPNUM);
	
  /* Check if the current endpoint can be written to. */
  if (Endpoint_ReadWriteAllowed()) {
    /* Process the endpoint bytes all at once; the audio is at such a
     * high sample rate that this does not have any noticable latency
     * on the USB management task */
    while (Endpoint_BytesInEndpoint() < AUDIO_STREAM_EPSIZE - (9 * sizeof(int16_t))) {

      /* Wait until next audio sample should be processed */
      if (!(TIFR0 & (1 << OCF0A)))
	continue;
      else
	TIFR0 |= (1 << OCF0A);

      for(int i = 0; i < AUDIO_CHANNELS - 1; i++) {
      /* Read the microphone board. This is not completely kosher -
       * the responses are out of sync with the requests. FIXME repair
       * in the interrupt version. */
	int sample = SPI_SendWord(ADC_CR_VAL | ADC_ADDR(i));

	  // USB Spec, "Frmts20 final.pdf" p16: left-justify the data.
	  // i.e. for 12 significant bits, shift right 4, have four 0 LSBs.
	  // Note we have to get the sign bit right.
	  //	  R = (R & 0x0FFF);
	Endpoint_Write_Word_LE(sample <<= 4);
      }

      /* Output a fairly slow square wave on the last channel. */
      int16_t square = val * gain[AUDIO_CHANNELS - 1] * 64;
/*       int16_t square = val * 128 * 64; */

      if(++slowdown >= 1024) {
	slowdown = 0;
	val = -val;
      }

      Endpoint_Write_Word_LE(square);
    }
		
    /* Send the full packet to the host */
    Endpoint_ClearCurrentBank();
  }
}
