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

#include <avr/interrupt.h>
#include <avr/power.h>

#include <stdbool.h>

#include "AudioInput.h"
#include "ADC.h"
#include "PreAmps.h"

/* Scheduler Task List */
TASK_LIST {
	{ Task: USB_USBTask, TaskStatus: TASK_STOP },
	{ Task: USB_Audio_Task       , TaskStatus: TASK_STOP },
};

int main(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable Clock Division */
	clock_prescale_set(clock_div_1);

	/* Initialise the ADC, and PreAmps. */
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
}

EVENT_HANDLER(USB_Disconnect)
{
	/* Stop running audio and USB management tasks */
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
static int16_t gain[AUDIO_CHANNELS] = {0x41, 0x42};//, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 101};

EVENT_HANDLER(USB_UnhandledControlPacket)
{
	unsigned char ucSREG;
	
	if ((bmRequestType & CONTROL_REQTYPE_TYPE) == REQTYPE_CLASS) {

		/* Audio-class specific. */
		switch (bmRequestType) {

			case AUDIO_REQ_TYPE_Get:
				switch (bRequest) {

					case AUDIO_REQ_GET_Cur:
					case AUDIO_REQ_GET_Min:
					case AUDIO_REQ_GET_Max:
					case AUDIO_REQ_GET_Res: {
						uint16_t wValue  = Endpoint_Read_Word_LE();
						uint16_t wIndex  = Endpoint_Read_Word_LE();
						uint16_t wLength = Endpoint_Read_Word_LE();

						/* A request for the current setting of a particular channel's input gain. */
						uint8_t ControlSelector = wValue >> 8;
						uint8_t ChannelNumber   = wValue & 0xFF;

						// FIXME no master control.
						// FIXME if ChannelNumber == 0xFF, then get all gain control settings.
						if (ControlSelector != FEATURE_VOLUME
								|| (ChannelNumber == 0 || ChannelNumber > AUDIO_CHANNELS)) {
							goto error;
						}

						/* Ensure the host is talking about the feature unit. */
						uint8_t Interface = wIndex & 0xFF;
						uint8_t EntityID  = wIndex >> 8;

						if (EntityID != FEATURE_UNIT_ID) {
							goto error;
						}

						// FIXME take care of wLength

						switch (bRequest) {
							case AUDIO_REQ_GET_Cur: {
								// Ask the pre-amp for the current setting.

								/*         gain[ChannelNumber - 1] = PreAmps_get(ChannelNumber - 1); */

								// FIXME the last channel is our square wave.
								if (ChannelNumber < AUDIO_CHANNELS) {
									//  gain[ChannelNumber - 1] = PreAmps_set(ChannelNumber - 1, 200);
									gain[ChannelNumber - 1] = PreAmps_get(ChannelNumber - 1);
								}

								Endpoint_ClearSetupReceived();
								Endpoint_Write_Control_Stream_LE(&gain[ChannelNumber - 1], sizeof(int16_t));
								Endpoint_ClearSetupOUT();
							}
							return;

							// FIXME adjust these in terms of dB's.
							// FIXME also ensure 0-255 is a valid range.
							case AUDIO_REQ_GET_Min: {
								int16_t min = 0x0000;

								Endpoint_ClearSetupReceived();
								Endpoint_Write_Control_Stream_LE(&min, sizeof(min));
								Endpoint_ClearSetupOUT();
							}
							return;

							case AUDIO_REQ_GET_Max: {
								int16_t max = 0x7FFF;

								Endpoint_ClearSetupReceived();
								Endpoint_Write_Control_Stream_LE(&max, sizeof(max));
								Endpoint_ClearSetupOUT();
							}
							return;

							case AUDIO_REQ_GET_Res: {
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
				switch (bRequest) {

					case AUDIO_REQ_SET_Cur: {
						uint16_t wValue  = Endpoint_Read_Word_LE();
						uint16_t wIndex  = Endpoint_Read_Word_LE();
						uint16_t wLength = Endpoint_Read_Word_LE();

						/* A request for the current setting of a particular channel's input gain. */
						uint8_t ControlSelector = wValue >> 8;
						uint8_t ChannelNumber   = wValue & 0xFF;

						// FIXME no master control.
						// FIXME if ChannelNumber == 0xFF, then get all gain control settings.
						if (ControlSelector != FEATURE_VOLUME
								|| (ChannelNumber == 0 || ChannelNumber > AUDIO_CHANNELS)) {
							goto error;
						}

						/* Ensure the host is talking about the feature unit. */
						uint8_t Interface = wIndex & 0xFF;
						uint8_t EntityID  = wIndex >> 8;

						if (EntityID != FEATURE_UNIT_ID) {
							goto error;
						}

						// FIXME take care of wLength

						Endpoint_ClearSetupReceived();
						Endpoint_Read_Control_Stream_LE(&gain[ChannelNumber - 1], sizeof(int16_t));
						Endpoint_ClearSetupIN();

						// FIXME Tell *all* the pre-amps for the new setting.
						for (int i = 0; i < 8; i++) {
							PreAmps_set(i, gain[ChannelNumber - 1]);
						}
						gain[AUDIO_CHANNELS - 1] = gain[ChannelNumber - 1];

						// Tell the pre-amp for the new setting.
						/*    if(ChannelNumber < AUDIO_CHANNELS) { */
						/*      PreAmps_set(ChannelNumber - 1, gain[ChannelNumber - 1]); */
						/*    } */

						return;
					}

					default:
						goto error;
				}
				break;

			default:
				goto error;
		}
	}
	else if (bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_STANDARD | REQREC_INTERFACE)) {
		switch (bRequest) {
			case REQ_SetInterface:
				/* Set Interface is not handled by the library, as its function is application-specific */
			{
				uint16_t wValue = Endpoint_Read_Word_LE();

				Endpoint_ClearSetupReceived();

				/* Check if the host is enabling the audio interface (setting AlternateSetting to 1) */
				if (wValue) {
					/* Clear (do not send) the audio isochronous endpoint buffer. */
					Endpoint_ResetFIFO(AUDIO_STREAM_EPNUM);

					// FIXME crank all the pre-amps to the max.
					for (int i = 0; i < 8; i++) {
						PreAmps_set(i, 0);
					}

					/* Tell the ADC we want to sample channel 0 on the next read. */
					ADC_ReadSampleAndSetNextAddr(0);

					/* Sample reload timer initialization */
					// disable interrupts to prevent race conditions with 16bit registers
					ucSREG = SREG;
					cli();
					// This is just an 16-bit counter.
					// So for an 8MHz clock AUDIO_SAMPLE_FREQUENCY must be > FIXME 31kHz.
					TCCR1B  = (1 << WGM12);  // Clear-timer-on-compare-match-OCR1A (CTC) mode
					TCCR1B |= (1 << CS10);  // Full FCPU speed
					OCR1A   = (uint16_t)(F_CPU / AUDIO_SAMPLE_FREQUENCY);
					// restore status register (will re-enable interrupts if the were enabled)
					SREG = ucSREG;

					/* Start audio task */
					Scheduler_SetTaskMode(USB_Audio_Task, TASK_RUN);
				}
				else {
					/* Stop audio task */
					Scheduler_SetTaskMode(USB_Audio_Task, TASK_STOP);
				
					/* Stop the sample reload timer */
					TCCR1B = 0;
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

/* SPI/ADC interrupt handler. We get woken up by the timer interrupt,
 * and try to shovel the samples from the ADC-via-SPI into the USB
 * endpoint buffer as quickly as possible.
 *
 * FIXME: eventually assume the ADC has already been told what's going
 * on and we're in sync with it. (i.e. stream the values out using the
 * ADC sequencer).
 *
 * FIXME: invariants: assume the ADC is setup to output channel 0 on
 * entry to the ISR. I don't think we need to use the sequencer if we
 * maintain this.
 *
 * Also assume there is enough room for a complete audio frame in the
 * endpoint buffer. This ensures we sample deterministically up to the
 * clock accuracy.
 *
 * Glitches: see audio formats doc, sec 2.2.1, p8, 2.2.4, p9.
 */
TASK(USB_Audio_Task)
{
	/* Save the current endpoint, restore it on exit. */
	uint8_t PrevEndpoint = Endpoint_GetCurrentEndpoint();

	/* Select the audio stream endpoint */
	Endpoint_SelectEndpoint(AUDIO_STREAM_EPNUM);

	/* Check if the current endpoint can be read from (contains a packet) */
	if (Endpoint_ReadWriteAllowed())
	{
		/* Wait until next audio sample should be processed 
			* (when the timer has gone off) */
		while (!(TIFR1 & (1 << OCF1A)));
		// reset the interrupt flag
		TIFR1 |= (1 << OCF1A);

		for (int i = 1; i < AUDIO_CHANNELS; i++) {
			// USB Spec, "Frmts20 final.pdf" p16: left-justify the data.
			// i.e. for 12 significant bits, shift right 4, have four 0 LSBs.
			// Note we have to get the sign bit right.
			Endpoint_Write_Word_LE(ADC_ReadSampleAndSetNextAddr(4*i));
		}

		/* Read the sample for the last mic channel and set it up to read
			* from channel 0 next time around. */
		Endpoint_Write_Word_LE(ADC_ReadSampleAndSetNextAddr(0));

		if (Endpoint_BytesInEndpoint() 
					 > AUDIO_STREAM_EPSIZE - (AUDIO_CHANNELS * sizeof(uint16_t))) {
			/* Send the full packet to the host */
			Endpoint_ClearCurrentBank();
		}
	}

	Endpoint_SelectEndpoint(PrevEndpoint);
}
