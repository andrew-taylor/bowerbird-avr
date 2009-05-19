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
// FIXME these should be updated when configuration changes
uint8_t active_config;
/** Array for storing cache calculations about what is the next channel
 * to sample for each unmuted channel. Muted channels have -1. */
uint8_t next_channel[MAX_AUDIO_CHANNELS];
// sampling frequency for all channels
uint32_t audio_sampling_frequency;
// channel gains in db
int16_t channel_volume[MAX_AUDIO_CHANNELS];


// forward declarations
void UpdateNextChannelArray(void);
void ResetADC(void);
void ConfigureSamplingTimer(uint32_t sampling_frequency);
void StartSamplingTimer(void);
void StopSamplingTimer(void);
uint8_t Volumes_Init(void);
static inline int16_t ConvertByteToVolume(uint8_t byte);
static inline uint8_t ConvertVolumeToByte(int16_t volume);
// void ProcessMuteRequest(uint8_t bRequest, uint8_t bmRequestType, uint8_t channelNumber);
void ProcessVolumeRequest(uint8_t bRequest, uint8_t bmRequestType, uint8_t channelNumber);
// void ProcessAutomaticGainRequest(uint8_t bRequest, uint8_t bmRequestType, uint8_t channelNumber);
void ProcessSamplingFrequencyRequest(uint8_t bRequest, uint8_t bmRequestType);
static inline void SendNAK(void);
static inline void ShowVal(uint16_t val);


void main(void) __ATTR_NORETURN__;
void main(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	// FIXME hack enable port a & c for debugging
	DDRA = 0xFF;
	DDRC = 0xFF;
	
	/* Disable Clock Division */
	clock_prescale_set(clock_div_1);

	/** by default, use the first configuration. */
	active_config = 0;
	
	/* The number of the active configuration */
	num_audio_channels = num_channels[active_config];
	
	/* Initialise the ADC, and PreAmps. */
	ADC_Init();
	PreAmps_Init();

	// set the default sampling frequency
	ConfigureSamplingTimer(DEFAULT_AUDIO_SAMPLE_FREQUENCY);

	// read all the volume values from the digital pots
	Volumes_Init();

	/* Initialize USB Subsystem */
	// FIXME disable USB gen vect, and poll for it in the loop below
	USB_Init();
	
	// run the background USB interfacing task (audio sampling is done by interrupt)
	while (1) {
		// disable interrupts to prevent race conditions with interrupt handler
// 		uint8_t SREG_save = SREG;
// 		cli();
// 		// check if USB General interrupts require processing
// 		if (USB_General_Interrupt_Requires_Processing()) {
// 			USB_Handle_General_Interrupt();
// 		}
// 		// restore interrupts
// 		SREG = SREG_save;

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
	/* Stop the sample reload timer (if it's running) */
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
			
			if (entityID == FEATURE_UNIT_ID1
					|| entityID == FEATURE_UNIT_ID2
					|| entityID == FEATURE_UNIT_ID4
					|| entityID == FEATURE_UNIT_ID8) {
				// determine the relevant control type
				controlSelector = wValue >> 8;
				// determine the channel
				channelNumber   = wValue & 0xFF;
				
				switch (controlSelector) {
					case FEATURE_VOLUME:
						ProcessVolumeRequest(bRequest, bmRequestType, channelNumber);
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

					// update cached & pre-calculated values
					active_config = wValue - 1;
					num_audio_channels = num_channels[active_config];
					// set up the next channel array for the interrupt handler
					UpdateNextChannelArray();
					/* Tell the ADC to sample the first unmuted channel on the next read. */
					ResetADC();

					/* Sample reload timer initialization */
					StartSamplingTimer();
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
	if (channelNumber == 0 || channelNumber > MAX_AUDIO_CHANNELS /*num_audio_channels*/) {
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
			// convert the USB channel number into our channels
			uint8_t our_channel_number = ADC_channels[active_config][channelNumber - 1];
			// set the pot
			PreAmps_set(our_channel_number, pot_value);
			return;
		}
		else {
			// FIXME send NAK?
			SendNAK();
		}
	}
}


void ProcessSamplingFrequencyRequest(uint8_t bRequest, uint8_t bmRequestType)
{
	uint8_t freq_byte[3];
	uint32_t sampling_frequency;
	
	// find out if its a "get" or a "set" request
	if (bmRequestType & AUDIO_REQ_TYPE_GET_MASK) {
		switch (bRequest) {
			case AUDIO_REQ_GET_Cur:
				freq_byte[2] = (audio_sampling_frequency >> 16) & 0xFF;
				freq_byte[1] = (audio_sampling_frequency >> 8) & 0xFF;
				freq_byte[0] = (audio_sampling_frequency & 0xFF);
				break;
			case AUDIO_REQ_GET_Min:
				freq_byte[2] = ((uint32_t)LOWEST_AUDIO_SAMPLE_FREQUENCY >> 16) & 0xFF;
				freq_byte[1] = ((uint32_t)LOWEST_AUDIO_SAMPLE_FREQUENCY >> 8) & 0xFF;
				freq_byte[0] = ((uint32_t)LOWEST_AUDIO_SAMPLE_FREQUENCY & 0xFF);
				break;
			case AUDIO_REQ_GET_Max:
				freq_byte[2] = ((uint32_t)HIGHEST_AUDIO_SAMPLE_FREQUENCY >> 16) & 0xFF;
				freq_byte[1] = ((uint32_t)HIGHEST_AUDIO_SAMPLE_FREQUENCY >> 8) & 0xFF;
				freq_byte[0] = ((uint32_t)HIGHEST_AUDIO_SAMPLE_FREQUENCY & 0xFF);
				break;
			case AUDIO_REQ_GET_Res:
				freq_byte[2] = 0;
				freq_byte[1] = 0;
				freq_byte[0] = 1;				
				break;
			default:
				// FIXME send NAK?
				SendNAK();
				return;
		}
		Endpoint_ClearSetupReceived();
		// FIXME check this is correct order to send the three bytes
		Endpoint_Write_Control_Stream(freq_byte, 3);
		Endpoint_ClearSetupOUT();
		return;
	}
	else {
		if (bRequest == AUDIO_REQ_SET_Cur) {
			/* A request for the current setting of a particular channel's input gain. */
			Endpoint_ClearSetupReceived();
			Endpoint_Read_Control_Stream(freq_byte, 3);
			Endpoint_ClearSetupIN();

			// update the sampling frequency
			sampling_frequency = ((uint32_t)(freq_byte[2]) << 16)
					| ((uint32_t)(freq_byte[1]) << 8)
					| ((uint32_t)(freq_byte[0]));
			
			// limit the frequency to our bounds
			if (sampling_frequency < LOWEST_AUDIO_SAMPLE_FREQUENCY)
				sampling_frequency = LOWEST_AUDIO_SAMPLE_FREQUENCY;
			else if (sampling_frequency > HIGHEST_AUDIO_SAMPLE_FREQUENCY)
				sampling_frequency = HIGHEST_AUDIO_SAMPLE_FREQUENCY;
			
			ConfigureSamplingTimer(sampling_frequency);
			return;
		}
	}

	// FIXME send NAK?
	SendNAK();
}


/** Update next_channel array for the interrupt handler.
 * This involves marking muted channels with -1, and the rest
 * with the next enabled channel to sample. */
void UpdateNextChannelArray(void)
{
	uint8_t i, j;

	for (i = 0; i < num_audio_channels; ++i) {
		j = (i + 1) % num_audio_channels;
		next_channel[i] = ADC_channels[active_config][j];
 	}
}


void ResetADC(void)
{
	ADC_ReadSampleAndSetNextAddr(ADC_channels[active_config][0]);

	/* setup the interrupt handler registers to point to their initial value */
	write_lsb = ADC_CR_LSB;
	write_msb = (ADC_CR_MSB | ADC_ADDR(next_channel[0]));
}


void ConfigureSamplingTimer(uint32_t sampling_frequency)
{
	unsigned char ucSREG;
	
	// disable interrupts to prevent race conditions with 16bit registers
	ucSREG = SREG;
	cli();

	OCR1A = (uint16_t)(F_CPU / sampling_frequency);

	// restore status register (will re-enable interrupts if the were enabled)
	SREG = ucSREG;
	
	// save current sampling frequency so we can report it later
	audio_sampling_frequency = sampling_frequency;
}


void StartSamplingTimer(void)
{
	TCCR1B  = (1 << WGM12)  // Clear-timer-on-compare-match-OCR1A (CTC) mode
			| (1 << CS10);  // Full FCPU speed
	TIMSK1 |= (1 << OCIE1A); // Enable timer interrupt
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
	
	for (uint8_t i = 0; i < num_audio_channels; ++i) {
		if (!PreAmps_get(i, &buf)) {
			// use half-max as fall-back if error occurs
			buf = 128;
			ret_val = 0;
		}
		channel_volume[i] = ConvertByteToVolume(buf);
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


static inline void ShowVal(uint16_t val)
{
	PORTA = ((val >> 8) & 0xFF);
	PORTC = (val & 0xFF);
}
