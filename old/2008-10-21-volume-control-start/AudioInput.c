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

#include "AudioInput.h"
#include <MyUSB/Drivers/AT90USBXXX/SPI.h>

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
/* 	ADC_Init(ADC_FREE_RUNNING | ADC_PRESCALE_32); */
/* 	ADC_SetupChannel(MIC_IN_ADC_CHANNEL); */
	
/* 	ADC_StartReading(ADC_REFERENCE_AVCC | ADC_RIGHT_ADJUSTED | MIC_IN_ADC_CHANNEL); */
	
	/* Initialize Scheduler so that it can be used */
	Scheduler_Init();

	/* Initialize USB Subsystem */
	USB_Init();

	/* Initialise the SPI bus: 4MHz clock (for a 8MHz AVR clock). FIXME this is as fast as it goes. */
	SPI_Init(SPI_USE_DOUBLESPEED, true);

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

/* Cheap-and-cheerful sinewave table. */
int16_t sine_table[256] = {
	0, 201, 402, 603, 803, 1003, 1202, 1400, 1598, 1795
	, 1990, 2185, 2378, 2569, 2759, 2948, 3135, 3319, 3502, 3683
	, 3861, 4037, 4211, 4382, 4551, 4716, 4879, 5039, 5196, 5350
	, 5501, 5648, 5792, 5932, 6069, 6202, 6332, 6457, 6579, 6697
	, 6811, 6920, 7026, 7127, 7224, 7316, 7405, 7488, 7567, 7642
	, 7712, 7778, 7838, 7894, 7946, 7992, 8034, 8070, 8102, 8129
	, 8152, 8169, 8181, 8189, 8191, 8189, 8181, 8169, 8152, 8129
	, 8102, 8070, 8034, 7992, 7946, 7894, 7838, 7778, 7712, 7642
	, 7567, 7488, 7405, 7316, 7224, 7127, 7026, 6920, 6811, 6697
	, 6579, 6457, 6332, 6202, 6069, 5932, 5792, 5648, 5501, 5350
	, 5196, 5039, 4879, 4716, 4551, 4382, 4211, 4037, 3861, 3683
	, 3502, 3319, 3135, 2948, 2759, 2569, 2378, 2185, 1990, 1795
	, 1598, 1400, 1202, 1003, 803, 603, 402, 201, 0, -201
	, -402, -603, -803, -1003, -1202, -1400, -1598, -1795, -1990, -2185
	, -2378, -2569, -2759, -2948, -3135, -3319, -3502, -3683, -3861, -4037
	, -4211, -4382, -4551, -4716, -4879, -5039, -5196, -5350, -5501, -5648
	, -5792, -5932, -6069, -6202, -6332, -6457, -6579, -6697, -6811, -6920
	, -7026, -7127, -7224, -7316, -7405, -7488, -7567, -7642, -7712, -7778
	, -7838, -7894, -7946, -7992, -8034, -8070, -8102, -8129, -8152, -8169
	, -8181, -8189, -8191, -8189, -8181, -8169, -8152, -8129, -8102, -8070
	, -8034, -7992, -7946, -7894, -7838, -7778, -7712, -7642, -7567, -7488
	, -7405, -7316, -7224, -7127, -7026, -6920, -6811, -6697, -6579, -6457
	, -6332, -6202, -6069, -5932, -5792, -5648, -5501, -5350, -5196, -5039
	, -4879, -4716, -4551, -4382, -4211, -4037, -3861, -3683, -3502, -3319
	, -3135, -2948, -2759, -2569, -2378, -2185, -1990, -1795, -1598, -1400
	, -1202, -1003, -803, -603, -402, -201
};

TASK(USB_Audio_Task)
{
  static int sample = 0;
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

	  int16_t L = sine_table[sample] * gain[1];
 	  int16_t R = val * gain[2] * 64;

	  if(++slowdown >= 1024) {
	    sample = (sample + 1) % 256; // (sizeof(sine_table));
	    slowdown = 0;
	    val = -val;
	  }

	  /* Write the sample to the buffer */
	  Endpoint_Write_Word_LE(L);
	  Endpoint_Write_Word_LE(R);
	}
		
      /* Send the full packet to the host */
      Endpoint_ClearCurrentBank();
    }
}
