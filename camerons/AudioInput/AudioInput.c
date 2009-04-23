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
#include <math.h>
#include <stdlib.h>

#include "Common.h"
#include "AudioInput.h"
#include "ADC.h"
#include "PreAmps.h"

#define DIGITAL_POT_RESISTANCE_BASE 75
#define DIGITAL_POT_RESISTANCE_MAX 100000
#define PREAMP_INPUT_RESISTANCE 1000
#define PREAMP_MINIMUM_SETPOINT ((uint8_t)3)
//((PREAMP_INPUT_RESISTANCE - DIGITAL_POT_RESISTANCE_BASE) * 0xFF / DIGITAL_POT_RESISTANCE_MAX))
#define PREAMP_MAXIMUM_SETPOINT ((uint8_t)0xFF)


/* Scheduler Task List */
TASK_LIST {
	{ Task: USB_USBTask, TaskStatus: TASK_STOP },
	{ Task: USB_Audio_Task       , TaskStatus: TASK_STOP },
};

// sampling frequency for all channels
int16_t audio_sampling_frequency;
// bool array of whether channels are muted
uint8_t channel_mute[AUDIO_CHANNELS];
// channel gains in db
int16_t channel_volume[AUDIO_CHANNELS];
// bool array of whether channels are set to have automatic gain
uint8_t channel_automatic_gain[AUDIO_CHANNELS];


// forward declarations
void InitialiseAndStartSamplingTimer(int16_t sampling_frequency);
void StopSamplingTimer(void);
uint8_t Volumes_Init(void);
static inline int16_t ConvertByteToVolume(uint8_t byte);
static inline uint8_t ConvertVolumeToByte(int16_t volume);
void ProcessMuteRequest(uint8_t bRequest, uint8_t bmRequestType, const uint8_t channelNumber);
void ProcessVolumeRequest(uint8_t bRequest, uint8_t bmRequestType, const uint8_t channelNumber);
void ProcessAutomaticGainRequest(uint8_t bRequest, uint8_t bmRequestType, const uint8_t channelNumber);
static inline void SendNAK(void);


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

	// set the default sampling frequency
	audio_sampling_frequency = DEFAULT_AUDIO_SAMPLE_FREQUENCY_KHZ;
	
	// read all the volume values from the digital pots
	Volumes_Init();

	/* Initialize Scheduler so that it can be used */
	Scheduler_Init();

	/* Initialize USB Subsystem */
	USB_Init();
	
	DDRC = 0xFF;
// 	asm ("loop: in __tmp_reg__, %[portc]\n\tcom __tmp_reg__\n\tout %[portc], __tmp_reg__\n\trjmp loop" : 
// 		: [portc] "I" (_SFR_IO_ADDR(PORTC)));
// 	while(1) {
// 		ADC_ReadSampleAndSetNextAddr(0);
// 	}

	/* Scheduling - routine never returns, so put this last in the main function */
	Scheduler_Start();
}


EVENT_HANDLER(USB_Connect)
{
	/* Start USB management task */
	Scheduler_SetTaskMode(USB_USBTask, TASK_RUN);
}


EVENT_HANDLER(USB_Disconnect)
{
	/* Stop running audio and USB management tasks */
	Scheduler_SetTaskMode(USB_USBTask, TASK_STOP);
}


EVENT_HANDLER(USB_ConfigurationChanged)
{
	/* Setup audio stream endpoint */
	Endpoint_ConfigureEndpoint(AUDIO_STREAM_EPNUM, EP_TYPE_ISOCHRONOUS,
							   ENDPOINT_DIR_IN, AUDIO_STREAM_EPSIZE,
							   ENDPOINT_BANK_DOUBLE);
}


EVENT_HANDLER(USB_UnhandledControlPacket)
{
	static uint16_t wValue, wIndex, wLength;
	static uint8_t controlSelector, channelNumber, entityID;

	if ((bmRequestType & CONTROL_REQTYPE_TYPE) == REQTYPE_CLASS)
	{
		wValue  = Endpoint_Read_Word();
		wIndex  = Endpoint_Read_Word();
		wLength = Endpoint_Read_Word();

		/* Ensure the host is talking about the feature unit. */
		entityID  = wIndex >> 8;
		if (entityID != FEATURE_UNIT_ID) {
			// FIXME confirm NAK should be sent here
			SendNAK();
			return;
		}

		// determine the relevant control type (mute, volume, auto-gain)
		controlSelector = wValue >> 8;
		// determine the channel
		channelNumber   = wValue & 0xFF;

		switch (controlSelector) {
			case FEATURE_MUTE:
				ProcessMuteRequest(bRequest, bmRequestType, channelNumber);
				break;
			case FEATURE_VOLUME:
				ProcessVolumeRequest(bRequest, bmRequestType, channelNumber);
				break;
			case FEATURE_AUTOMATIC_GAIN:
				ProcessAutomaticGainRequest(bRequest, bmRequestType, channelNumber);
				break;
			default:
				SendNAK();
				return;
		}
	}
	else if (bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_STANDARD | REQREC_INTERFACE)) {
		switch (bRequest) {
			case REQ_SetInterface:
			{
				uint16_t wValue = Endpoint_Read_Word();
				Endpoint_ClearSetupReceived();

				/* Check if the host is enabling the audio interface (setting AlternateSetting to 1) */
				if (wValue) {
					/* Clear (do not send) the audio isochronous endpoint buffer. */
					Endpoint_ResetFIFO(AUDIO_STREAM_EPNUM);

					/* Tell the ADC we want to sample channel 0 on the next read. */
					ADC_ReadSampleAndSetNextAddr(0);

					/* Sample reload timer initialization */
					InitialiseAndStartSamplingTimer(audio_sampling_frequency);

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
	
	// FIXME confirm NAK should be sent here
	SendNAK();
}


void ProcessVolumeRequest(const uint8_t bRequest, const uint8_t bmRequestType,
		const uint8_t channelNumber)
{
	uint8_t buf;
	int16_t value;
	
	// FIXME no master control.
	// FIXME if channelNumber == 0xFF, then get all gain control settings.
	if (channelNumber == 0 || channelNumber > AUDIO_CHANNELS) {
		SendNAK();
		return;
	}

	switch (bmRequestType)
	{
		case AUDIO_REQ_TYPE_Get:
			switch (bRequest)
			{
				case AUDIO_REQ_GET_Cur:
					// FIXME this will be unneccessary once the preamp code actually works
					if (PreAmps_get(channelNumber - 1, &buf)) {
						// use approximate half-max as fall-back if error occurs
						buf = 0x1f;
					}
 					channel_volume[channelNumber - 1] = ConvertByteToVolume(buf);
					value = channel_volume[channelNumber - 1];
					Endpoint_ClearSetupReceived();
					Endpoint_Write_Control_Stream(&value, sizeof(value));
					Endpoint_ClearSetupOUT();
					return;
				case AUDIO_REQ_GET_Min:
					value = ConvertByteToVolume(PREAMP_MINIMUM_SETPOINT);
					Endpoint_ClearSetupReceived();
					Endpoint_Write_Control_Stream(&value, sizeof(value));
					Endpoint_ClearSetupOUT();
					return;
				case AUDIO_REQ_GET_Max:
					value = ConvertByteToVolume(PREAMP_MAXIMUM_SETPOINT);
					Endpoint_ClearSetupReceived();
					Endpoint_Write_Control_Stream(&value, sizeof(value));
					Endpoint_ClearSetupOUT();
					return;
				case AUDIO_REQ_GET_Res:
					value = 1;
					Endpoint_ClearSetupReceived();
					Endpoint_Write_Control_Stream(&value, sizeof(value));
					Endpoint_ClearSetupOUT();
					return;
			}
			break;

		case AUDIO_REQ_TYPE_Set:
			if (bRequest == AUDIO_REQ_SET_Cur) {
				/* A request for the current setting of a particular channel's input gain. */
				Endpoint_ClearSetupReceived();
				Endpoint_Read_Control_Stream(&value, sizeof(value));
				Endpoint_ClearSetupIN();

				// cache the value
				channel_volume[channelNumber - 1] = value;
				// convert to a digital pot value
				uint8_t pot_value = ConvertVolumeToByte(value);
				// set the pot
				PreAmps_set(channelNumber - 1, pot_value);
				return;
			}
			break;
	}

	// FIXME send NACK?
	SendNAK();
}


void ProcessMuteRequest(const uint8_t bRequest, const uint8_t bmRequestType,
		const uint8_t channelNumber)
{
	// FIXME no master control.
	// FIXME if channelNumber == 0xFF, then get all gain control settings.
	if (channelNumber == 0 || channelNumber > AUDIO_CHANNELS) {
		SendNAK();
		return;
	}

	switch (bmRequestType)
	{
		case AUDIO_REQ_TYPE_Get:
			if (bRequest == AUDIO_REQ_GET_Cur) {
				uint8_t muted = channel_mute[channelNumber - 1];
				Endpoint_ClearSetupReceived();
				Endpoint_Write_Control_Stream(&muted, sizeof(muted));
				Endpoint_ClearSetupOUT();
				return;
			}
			break;
		case AUDIO_REQ_TYPE_Set:
			if (bRequest == AUDIO_REQ_SET_Cur) {
				/* A request for the current setting of a particular channel's input gain. */
				uint8_t muted;
				Endpoint_ClearSetupReceived();
				Endpoint_Read_Control_Stream(&muted, sizeof(muted));
				Endpoint_ClearSetupIN();

				// cache the value
				channel_mute[channelNumber - 1] = muted;
				return;
			}
			break;
	}

	// FIXME send nak?
	SendNAK();
}


void ProcessAutomaticGainRequest(const uint8_t bRequest, const uint8_t bmRequestType,
		const uint8_t channelNumber)
{
	// FIXME no master control.
	// FIXME if channelNumber == 0xFF, then get all gain control settings.
	if (channelNumber == 0 || channelNumber > AUDIO_CHANNELS) {
		SendNAK();
		return;
	}

	switch (bmRequestType)
	{
		case AUDIO_REQ_TYPE_Get:
			if (bRequest == AUDIO_REQ_GET_Cur) {
				uint8_t auto_gain = channel_automatic_gain[channelNumber - 1];
				Endpoint_ClearSetupReceived();
				Endpoint_Write_Control_Stream(&auto_gain, sizeof(auto_gain));
				Endpoint_ClearSetupOUT();
				return;
			}
			break;
		case AUDIO_REQ_TYPE_Set:
			if (bRequest == AUDIO_REQ_SET_Cur) {
				/* A request for the current setting of a particular channel's input gain. */
				uint8_t auto_gain;
				Endpoint_ClearSetupReceived();
				Endpoint_Read_Control_Stream(&auto_gain, sizeof(auto_gain));
				Endpoint_ClearSetupIN();

				// cache the value
				channel_automatic_gain[channelNumber - 1] = auto_gain;
				return;
			}
			break;
	}

	// FIXME send nak?
	SendNAK();
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
		
		for (int i = 1; i < AUDIO_CHANNELS; i++) {
// 			if (!channel_mute[i]) {
				// USB Spec, "Frmts20 final.pdf" p16: left-justify the data.
				// i.e. for 12 significant bits, shift right 4, have four 0 LSBs.
				// Note we have to get the sign bit right.
				Endpoint_Write_Word(ADC_ReadSampleAndSetNextAddr(i));
// 			}
		}

		/* Read the sample for the last mic channel and set it up to read
			* from channel 0 next time around. */
		Endpoint_Write_Word(ADC_ReadSampleAndSetNextAddr(0));

		if (Endpoint_BytesInEndpoint() 
					 > AUDIO_STREAM_EPSIZE - (AUDIO_CHANNELS * sizeof(uint16_t))) {
			/* Send the full packet to the host */
			Endpoint_ClearCurrentBank();
		}

		// reset the interrupt flag
		TIFR1 |= (1 << OCF1A);
	}

	Endpoint_SelectEndpoint(PrevEndpoint);
}


void InitialiseAndStartSamplingTimer(int16_t sampling_frequency_khz)
{
	unsigned char ucSREG;
	
	// disable interrupts to prevent race conditions with 16bit registers
	ucSREG = SREG;
	cli();
	TCCR1B  = (1 << WGM12)  // Clear-timer-on-compare-match-OCR1A (CTC) mode
			| (1 << CS10);  // Full FCPU speed
	OCR1A   = (uint16_t)(F_CPU_KHZ / sampling_frequency_khz);
	// restore status register (will re-enable interrupts if the were enabled)
	SREG = ucSREG;
}


void StopSamplingTimer(void)
{
	TCCR1B &= ~(1 << CS10) & ~(1 << CS11) & ~(1 << CS12);
}


// Returns 0 on failure, 1 on success.
uint8_t Volumes_Init(void)
{
	uint8_t buf, ret_val = 1;
	
	for (uint8_t i = 0; i < AUDIO_CHANNELS; ++i) {
		if (!PreAmps_get(i, &buf)) {
			// use half-max as fall-back if error occurs
			buf = 128;
			ret_val = 0;
		}
		channel_volume[i] = ConvertByteToVolume(buf);
		channel_mute[i] = (buf == 0);

		// initialise auto gain to false
		channel_automatic_gain[i] = 0;
	}

	return ret_val;
}


// Digital pots (feedback resistance) range between ~0 and 100k with 256 values
// Input resistor is 1k, and voltage gain is input/feedback
// db gain (volume) is 20 log10(voltage gain)
static inline int16_t ConvertByteToVolume(uint8_t byte)
{
	// convert byte setting for the digital pot into resistor value
	double feedback_resistance = DIGITAL_POT_RESISTANCE_BASE + byte * (DIGITAL_POT_RESISTANCE_MAX - DIGITAL_POT_RESISTANCE_BASE) / 0xff;
	// convert feedback resistor value into a voltage gain
	double voltage_gain = feedback_resistance / PREAMP_INPUT_RESISTANCE;
	// convert voltage gain into a db (logarithmic) gain
	return 20 * log10(voltage_gain);
}


static inline uint8_t ConvertVolumeToByte(int16_t volume)
{
	// convert db (logarithmic) gain into a voltage gain;
	double voltage_gain = pow(10, volume / 20.0);
	// convert the voltage gain into a feedback resistance value
	double feedback_resistance = voltage_gain * PREAMP_INPUT_RESISTANCE;
	// convert feedback resistance into a byte setting for the digital pot
	double pot_setting = (feedback_resistance - DIGITAL_POT_RESISTANCE_BASE) * 0xff / (DIGITAL_POT_RESISTANCE_MAX - DIGITAL_POT_RESISTANCE_BASE);
	// limit value for digital pot
	return pot_setting > 0xff ? 0xff : pot_setting < 0 ? : (uint8_t)pot_setting;
}


static inline void SendNAK(void)
{
	//FIXME send a NACK to the host
}