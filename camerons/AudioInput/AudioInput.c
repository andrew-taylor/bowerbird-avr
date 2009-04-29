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

/* Includes: */
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>

#include "Descriptors.h"
#include "ADC.h"
#include "PreAmps.h"

#include <MyUSB/Version.h>                      // Library Version Information
#include <MyUSB/Drivers/USB/USB.h>              // USB Functionality
#include <MyUSB/Drivers/USB/LowLevel/DevChapter9.h>
#include <MyUSB/Drivers/USB/HighLevel/USBInterrupt.h>

/* digital pot values used to calculate gains */
#define DIGITAL_POT_RESISTANCE_BASE 75
#define DIGITAL_POT_RESISTANCE_MAX 100000
#define PREAMP_INPUT_RESISTANCE 1000
#define PREAMP_MINIMUM_SETPOINT ((uint8_t)3)
//FIXME should be calculated but this calc doesn't work
// #define PREAMP_MINIMUM_SETPOINT ((PREAMP_INPUT_RESISTANCE - DIGITAL_POT_RESISTANCE_BASE) * 0xFF / DIGITAL_POT_RESISTANCE_MAX))
#define PREAMP_MAXIMUM_SETPOINT ((uint8_t)0xFF)

/* Event Handlers: */
HANDLES_EVENT(USB_Connect);
HANDLES_EVENT(USB_Disconnect);
HANDLES_EVENT(USB_ConfigurationChanged);
HANDLES_EVENT(USB_Suspend);
HANDLES_EVENT(USB_UnhandledControlPacket);



// FIXME put all global variables into a struct
// (makes it faster to reference them)
// sampling frequency for all channels
uint32_t current_audio_sampling_frequency;
// this value is used to delay update until next recording starts
uint32_t next_audio_sampling_frequency;
// bool array of whether channels are muted
uint8_t channel_mute[AUDIO_CHANNELS];
// channel gains in db
int16_t channel_volume[AUDIO_CHANNELS];
// bool array of whether channels are set to have automatic gain
uint8_t channel_automatic_gain[AUDIO_CHANNELS];


// forward declarations
void InitialiseAndStartSamplingTimer(uint32_t sampling_frequency);
void StopSamplingTimer(void);
uint8_t Volumes_Init(void);
static inline int16_t ConvertByteToVolume(uint8_t byte);
static inline uint8_t ConvertVolumeToByte(int16_t volume);
void ProcessMuteRequest(uint8_t bRequest, uint8_t bmRequestType, uint8_t channelNumber);
void ProcessVolumeRequest(uint8_t bRequest, uint8_t bmRequestType, uint8_t channelNumber);
void ProcessAutomaticGainRequest(uint8_t bRequest, uint8_t bmRequestType, uint8_t channelNumber);
void ProcessSamplingFrequencyRequest(uint8_t bRequest, uint8_t bmRequestType);
static inline void SendNAK(void);


void main(void) __ATTR_NORETURN__;

void main(void)
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
	next_audio_sampling_frequency = DEFAULT_AUDIO_SAMPLE_FREQUENCY;
	
	// read all the volume values from the digital pots
	Volumes_Init();

	/* Initialize USB Subsystem */
	// FIXME disable USB gen vect, and poll for it in the loop below
	USB_Init();
	
	// FIXME hack enable port a & c for debugging
	DDRA = 0xFF;
	DDRC = 0xFF;

	// run the background task (audio sampling done by interrupt)
	while (1) {
		// check if USB General interrupts require processing
		while (USB_General_Interrupt_Requires_Processing()) {
			USB_Handle_General_Interrupt();
		}

		// handle any control packets received
		if (USB_IsConnected) {
			uint8_t PrevEndpoint = Endpoint_GetCurrentEndpoint();
	
			Endpoint_SelectEndpoint(ENDPOINT_CONTROLEP);
	
			if (Endpoint_IsSetupReceived())
				USB_Device_ProcessControlPacket();
	
			Endpoint_SelectEndpoint(PrevEndpoint);
		}
	}
}


EVENT_HANDLER(USB_Connect)
{
	// no op
}


EVENT_HANDLER(USB_Disconnect)
{
	/* Stop running audio and USB management tasks */
// 	Scheduler_SetTaskMode(USB_USBTask, TASK_STOP);

	/* Stop the sample reload timer */
	StopSamplingTimer();
}


EVENT_HANDLER(USB_ConfigurationChanged)
{
	/* Setup audio stream endpoint */
	Endpoint_ConfigureEndpoint(AUDIO_STREAM_EPNUM, EP_TYPE_ISOCHRONOUS,
							   ENDPOINT_DIR_IN, AUDIO_STREAM_EPSIZE,
							   ENDPOINT_BANK_DOUBLE);
}


// FIXME can we create more specific handlers instead of all of them in this?
EVENT_HANDLER(USB_UnhandledControlPacket)
{
	static uint16_t recipient, wValue, wIndex, wLength;
	static uint8_t controlSelector, channelNumber, entityID;

	if ((bmRequestType & CONTROL_REQTYPE_TYPE) == REQTYPE_CLASS)
	{
		recipient = (bmRequestType & CONTROL_REQTYPE_RECIPIENT);
		wValue  = Endpoint_Read_Word();
		wIndex  = Endpoint_Read_Word();
		wLength = Endpoint_Read_Word();		
		
		if (recipient == REQREC_ENDPOINT) {
			// determine the relevant control type
			controlSelector = wValue >> 8;

			// FIXME do we need to confirm that the endpoint in wIndex is our endpoint?
			if (controlSelector == SAMPLING_FREQ_CONTROL) {
				ProcessSamplingFrequencyRequest(bRequest, bmRequestType);
			}
			else {
				// spec (Audio10.pdf p95) says we have to send stall here
				Endpoint_StallTransaction();
				return;
			}
		}	
		else if (recipient == REQREC_INTERFACE) {
			// get the entity id
			entityID  = wIndex >> 8;
			
			if (entityID == FEATURE_UNIT_ID) {
				// determine the relevant control type
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
		}
	}
	else if (bmRequestType ==
			(REQDIR_HOSTTODEVICE | REQTYPE_STANDARD | REQREC_INTERFACE)) {
		switch (bRequest) {
			case REQ_SetInterface:
			{
				uint16_t wValue = Endpoint_Read_Word();
				Endpoint_ClearSetupReceived();

				/* Check if the host is enabling the audio interface
				 * (setting AlternateSetting to 1) */
				if (wValue) {
					/* Clear the audio isochronous endpoint buffer. */
					Endpoint_ResetFIFO(AUDIO_STREAM_EPNUM);

					/* Tell the ADC to sample channel 0 on the next read. */
					ADC_ReadSampleAndSetNextAddr(0);

					/* Sample reload timer initialization */
					InitialiseAndStartSamplingTimer(next_audio_sampling_frequency);
				}
				else {
					/* Stop the sample reload timer */
					StopSamplingTimer();
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


void ProcessVolumeRequest(uint8_t bRequest, uint8_t bmRequestType,
		uint8_t channelNumber)
{
	uint8_t buf;
	int16_t value;
	
	// FIXME no master control.
	// FIXME if channelNumber == 0xFF, then get all gain control settings.
	if (channelNumber == 0 || channelNumber > AUDIO_CHANNELS) {
		SendNAK();
		return;
	}

	// find out if its a "get" or a "set" request
	if (bmRequestType & AUDIO_REQ_TYPE_GET_MASK) {
		switch (bRequest) {
			case AUDIO_REQ_GET_Cur:
				// FIXME this will be unneccessary once the preamp code actually works
				if (PreAmps_get(channelNumber - 1, &buf)) {
					// use approximate half-max as fall-back if error occurs
					buf = 0x1f;
				}
				channel_volume[channelNumber - 1] = ConvertByteToVolume(buf);
				value = channel_volume[channelNumber - 1];
				break;
			case AUDIO_REQ_GET_Min:
				value = ConvertByteToVolume(PREAMP_MINIMUM_SETPOINT);
				break;
			case AUDIO_REQ_GET_Max:
				value = ConvertByteToVolume(PREAMP_MAXIMUM_SETPOINT);
				break;
			case AUDIO_REQ_GET_Res:
				value = 1;
				break;
			default:
				// FIXME send NAK?
				SendNAK();
				return;
		}
		Endpoint_ClearSetupReceived();
		Endpoint_Write_Control_Stream(&value, sizeof(value));
		Endpoint_ClearSetupOUT();
	}
	else {
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
		else {
			// FIXME send NAK?
			SendNAK();
		}
	}
}


void ProcessMuteRequest(uint8_t bRequest, uint8_t bmRequestType,
		uint8_t channelNumber)
{
	uint8_t muted;
	
	// FIXME no master control.
	// FIXME if channelNumber == 0xFF, then get all gain control settings.
	if (channelNumber == 0 || channelNumber > AUDIO_CHANNELS) {
		SendNAK();
		return;
	}

	// find out if its a "get" or a "set" request
	if (bmRequestType & AUDIO_REQ_TYPE_GET_MASK) {
		if (bRequest == AUDIO_REQ_GET_Cur) {
			muted = channel_mute[channelNumber - 1];
			Endpoint_ClearSetupReceived();
			Endpoint_Write_Control_Stream(&muted, sizeof(muted));
			Endpoint_ClearSetupOUT();
			return;
		}
	}
	else {
		if (bRequest == AUDIO_REQ_SET_Cur) {
			/* A request for the current setting of a particular channel's input gain. */
			Endpoint_ClearSetupReceived();
			Endpoint_Read_Control_Stream(&muted, sizeof(muted));
			Endpoint_ClearSetupIN();

			// cache the value
			channel_mute[channelNumber - 1] = muted;
			return;
		}
	}

	// FIXME send NAK?
	SendNAK();
}


void ProcessAutomaticGainRequest(uint8_t bRequest, uint8_t bmRequestType,
		uint8_t channelNumber)
{
	uint8_t auto_gain;
	
	// FIXME no master control.
	// FIXME if channelNumber == 0xFF, then get all gain control settings.
	if (channelNumber == 0 || channelNumber > AUDIO_CHANNELS) {
		SendNAK();
		return;
	}

	// find out if its a "get" or a "set" request
	if (bmRequestType & AUDIO_REQ_TYPE_GET_MASK) {
		if (bRequest == AUDIO_REQ_GET_Cur) {
			auto_gain = channel_automatic_gain[channelNumber - 1];
			Endpoint_ClearSetupReceived();
			Endpoint_Write_Control_Stream(&auto_gain, sizeof(auto_gain));
			Endpoint_ClearSetupOUT();
			return;
		}
	}
	else {
		if (bRequest == AUDIO_REQ_SET_Cur) {
			/* A request for the current setting of a particular channel's input gain. */
			Endpoint_ClearSetupReceived();
			Endpoint_Read_Control_Stream(&auto_gain, sizeof(auto_gain));
			Endpoint_ClearSetupIN();

			// cache the value
			channel_automatic_gain[channelNumber - 1] = auto_gain;
			return;
		}
	}

	// FIXME send NAK?
	SendNAK();
}


void ProcessSamplingFrequencyRequest(uint8_t bRequest, uint8_t bmRequestType)
{
	uint8_t freq_high_byte;
	int16_t freq_low_word;
	// FIXME hacked removal
	return;
	
	// find out if its a "get" or a "set" request
	if (bmRequestType & AUDIO_REQ_TYPE_GET_MASK) {
		switch (bRequest) {
			case AUDIO_REQ_GET_Cur:
				freq_high_byte = SAMPLE_FREQ_HIGH_BYTE(current_audio_sampling_frequency);
				freq_low_word = SAMPLE_FREQ_LOW_WORD(current_audio_sampling_frequency);
				break;
			case AUDIO_REQ_GET_Min:
				freq_high_byte = SAMPLE_FREQ_HIGH_BYTE(LOWEST_AUDIO_SAMPLE_FREQUENCY);
				freq_low_word = SAMPLE_FREQ_LOW_WORD(LOWEST_AUDIO_SAMPLE_FREQUENCY);
				break;
			case AUDIO_REQ_GET_Max:
				freq_high_byte = SAMPLE_FREQ_HIGH_BYTE(HIGHEST_AUDIO_SAMPLE_FREQUENCY);
				freq_low_word = SAMPLE_FREQ_LOW_WORD(HIGHEST_AUDIO_SAMPLE_FREQUENCY);
				break;
			case AUDIO_REQ_GET_Res:
				freq_high_byte = 0;
				freq_low_word = 1;
				break;
			default:
				// FIXME send NAK?
				SendNAK();
				return;
		}
		Endpoint_ClearSetupReceived();
		// FIXME check this is correct order to send the three bytes
		Endpoint_Write_Control_Stream(&freq_low_word, sizeof(freq_low_word));
		Endpoint_Write_Control_Stream(&freq_high_byte, sizeof(freq_high_byte));
		Endpoint_ClearSetupOUT();
		return;
	}
	else {
		if (bRequest == AUDIO_REQ_SET_Cur) {
			/* A request for the current setting of a particular channel's input gain. */
			Endpoint_ClearSetupReceived();
			freq_low_word = Endpoint_Read_Word();
			freq_high_byte = Endpoint_Read_Byte();
			Endpoint_ClearSetupIN();

			// update the sampling frequency for next recording
			next_audio_sampling_frequency = ((uint32_t)freq_high_byte << 16) & freq_low_word;
			return;
		}
	}

	// FIXME send NAK?
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
// ISR(TIMER1_COMPA_vect, ISR_BLOCK)
// {
// 	PORTA = 0xFF;
// 	/* Save the current endpoint, restore it on exit. */
// 	uint8_t PrevEndpoint = Endpoint_GetCurrentEndpoint();
// 
// 	/* Select the audio stream endpoint */
// 	Endpoint_SelectEndpoint(AUDIO_STREAM_EPNUM);
// 
// 	/* Check if the current endpoint can be read from (contains a packet) */
// 	if (Endpoint_ReadWriteAllowed()) {
// 		int i = 0;
// 		do {
// 			i++;
// 			if (i == AUDIO_CHANNELS)
// 				i = 0;
// 			if (channel_mute[i]) {
// 				// send zero length packet (PrevEndpoint is not used)
// // 				Endpoint_Write_Control_Stream(&PrevEndpoint, 0);
// 				Endpoint_Write_Word(0);
// 			}
// 			else {
// 				// USB Spec, "Frmts20 final.pdf" p16: left-justify the data.
// 				// i.e. for 12 significant bits, shift right 4, have four 0 LSBs.
// 				// Note we have to get the sign bit right.
// 				Endpoint_Write_Word(ADC_ReadSampleAndSetNextAddr(i));
// 			}
// 		} while (i != 0);
// 
// 		if (Endpoint_BytesInEndpoint() > AUDIO_STREAM_FULL_THRESHOLD) {
// 			PORTC = ~PORTC;
// 			/* Send the full packet to the host */
// 			Endpoint_ClearCurrentBank();
// 		}
// 	}
// 
// 	Endpoint_SelectEndpoint(PrevEndpoint);
// 	PORTA = 0;
// }


void InitialiseAndStartSamplingTimer(uint32_t sampling_frequency)
{
	unsigned char ucSREG;
	
	// disable interrupts to prevent race conditions with 16bit registers
	ucSREG = SREG;
	cli();

	TCCR1B  = (1 << WGM12)  // Clear-timer-on-compare-match-OCR1A (CTC) mode
			| (1 << CS10);  // Full FCPU speed
	OCR1A   = (uint16_t)(F_CPU / sampling_frequency);
	TIMSK1 |= (1 << OCIE1A); // Enable timer interrupt

	// restore status register (will re-enable interrupts if the were enabled)
	SREG = ucSREG;

	// save current sampling frequency so we can report it later
	current_audio_sampling_frequency = sampling_frequency;
}


void StopSamplingTimer(void)
{
	TIMSK1 &= ~(1 << OCIE1A); // Disable timer interrupt
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
	//FIXME send a NAK to the host
}
