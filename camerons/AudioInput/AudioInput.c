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
#define PREAMP_INPUT_RESISTANCE 560
#define PREAMP_MINIMUM_SETPOINT ((uint8_t)3)
//FIXME should be calculated but this calc doesn't work
// #define PREAMP_MINIMUM_SETPOINT ((PREAMP_INPUT_RESISTANCE - DIGITAL_POT_RESISTANCE_BASE) * 0xFF / DIGITAL_POT_RESISTANCE_MAX))
#define PREAMP_MAXIMUM_SETPOINT ((uint8_t)0xFF)

/** The number of channels in each alternate setting.
 * NOTE: these much match the values in the descriptors
 * (as well as the Mic arrays below) */
uint8_t num_channels[NUM_ALTERNATE_SETTINGS] = { 1, 2, 4, 8 };

/** The number of microphone configurations supported for each alternate
 * setting. The different configurations are chosen using the Selector Unit.
 * NOTE: these much match the values in the descriptors
 * (as well as the Mic arrays below) */
uint8_t num_selections[NUM_ALTERNATE_SETTINGS] = { 4, 4, 5, 1 };

/** The actual microphone numbers (in terms of the actual mic board) for each
 * number of channels & configuration chosen.
 * NOTE: Microphone numbers range from 0 to 7. */
uint8_t selections_1[][1] = { { 0 }, { 2 }, { 4 }, { 6 } };
uint8_t selections_2[][2] = { { 0, 2 }, { 2, 4 }, { 4, 6 }, { 6, 0 } };
uint8_t selections_4[][4] = { { 0, 2, 4, 6 }, { 0, 1, 2, 3 }, { 2, 3, 4, 5 }, { 4, 5, 6, 7 }, { 6, 7, 0, 1 } };
uint8_t selections_8[][8] = { { 0, 1, 2, 3, 4, 5, 6, 7 } };


/* Event Handlers: */
HANDLES_EVENT(USB_Connect);
HANDLES_EVENT(USB_Disconnect);
HANDLES_EVENT(USB_ConfigurationChanged);
HANDLES_EVENT(USB_Suspend);
HANDLES_EVENT(USB_UnhandledControlPacket);


// FIXME put all global variables into a struct
// (makes it faster to reference them)
/** Array for storing cache calculations about what is the next channel
 * to sample for each unmuted channel. Muted channels have -1. */
uint8_t next_channel[MAX_AUDIO_CHANNELS];
/** sampling frequency for all channels */
uint32_t audio_sampling_frequency;
/** channel gains in db */
int16_t channel_volume[MAX_AUDIO_CHANNELS];
/** current selector unit values */
uint8_t selector_unit[NUM_ALTERNATE_SETTINGS];


// forward declarations
uint8_t GetMicrophoneIndex(uint8_t channel, uint8_t alternateSetting);
void UpdateNextChannelArray(uint8_t alternateSetting);
void ResetADC(uint8_t alternateSetting);
void ConfigureSamplingTimer(uint32_t sampling_frequency);
void StartSamplingTimer(void);
void StopSamplingTimer(void);
uint8_t Volumes_Init(void);
static inline int16_t ConvertByteToVolume(uint8_t byte);
static inline uint8_t ConvertVolumeToByte(int16_t volume);
void ProcessSelectorRequest(uint8_t bRequest, uint8_t bmRequestType, uint8_t entityId);
void ProcessVolumeRequest(uint8_t bRequest, uint8_t bmRequestType, uint8_t entityId, uint8_t channelNumber);
void ProcessSamplingFrequencyRequest(uint8_t bRequest, uint8_t bmRequestType);
static inline void SendNAK(void);
static inline void ShowVal(uint16_t val);


int main(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable Clock Division */
	clock_prescale_set(clock_div_1);

	// Enable pull-ups on unused ports to reduce power consumption
	PORTA = 0xFF;
	PORTC = 0xFF;
	PORTE = 0xFF;
	PORTF = 0xFF;

	// Enable pull-ups on unused pins of PORTB
	// (Pins 1,2,3 & 4 are used by SPI (counting from 1))
	PORTB = 0xF0;
	// Enable pull-ups on unused pins of PORTD 
	// (Pins 1,2 & 8 are used by I2C (TWI) (counting from 1))
	PORTD = 0x7C;
	
	/* these will be updated when a recording is started */
	num_audio_channels = 0;
	multichannel = 0;
	
	/* Initialise the ADC, and PreAmps. */
	ADC_Init();
	PreAmps_Init();

	// set the default sampling frequency
	ConfigureSamplingTimer(DEFAULT_AUDIO_SAMPLE_FREQUENCY);

	// read all the volume values from the digital pots
	Volumes_Init();

	// initialise all selector units to their first value
	for (int i = 0; i < NUM_ALTERNATE_SETTINGS; ++i) {
		selector_unit[i] = 0;
	}
	
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

	return 0;
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
	static uint8_t controlSelector, channelNumber, entityId;

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
			entityId  = wIndex >> 8;
			
			if (entityId == SELECTOR_UNIT_ID1
			   		|| entityId == SELECTOR_UNIT_ID2
			   		|| entityId == SELECTOR_UNIT_ID4) {
				ProcessSelectorRequest(bRequest, bmRequestType, entityId);
			}
			else if (entityId == FEATURE_UNIT_ID1
					|| entityId == FEATURE_UNIT_ID2
					|| entityId == FEATURE_UNIT_ID4
					|| entityId == FEATURE_UNIT_ID8) {
				// determine the relevant control type
				controlSelector = wValue >> 8;
				// determine the channel
				channelNumber   = wValue & 0xFF;
				
				switch (controlSelector) {
					case FEATURE_VOLUME:
						ProcessVolumeRequest(bRequest, bmRequestType, entityId, channelNumber);
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
				uint16_t wInterface = Endpoint_Read_Word();
				Endpoint_ClearSetupReceived();

				/* Check if the host is enabling the audio interface
				 * (setting AlternateSetting to 1) */
				if (wInterface) {
					/* Clear the audio isochronous endpoint buffer. */
					Endpoint_ResetFIFO(AUDIO_STREAM_EPNUM);
					bytes_in_usb_buffer = 0;

					// update cached & pre-calculated values
					uint8_t alternate_setting = wInterface - 1;
					num_audio_channels = num_channels[alternate_setting];
					multichannel = num_audio_channels - 1;
					// set up the next channel array for the interrupt handler
					UpdateNextChannelArray(alternate_setting);
					/* Tell the ADC to sample the first unmuted channel on the next read. */
					ResetADC(alternate_setting);

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


void ProcessSelectorRequest(uint8_t bRequest, uint8_t bmRequestType,
		uint8_t entityId)
{
	uint8_t value;
	
	uint8_t selector_index = entityId == SELECTOR_UNIT_ID1 ? 0
			: entityId == SELECTOR_UNIT_ID2 ? 1
			: entityId == SELECTOR_UNIT_ID4 ? 2
			: 0;
	
	// find out if its a "get" or a "set" request
	if (bmRequestType & AUDIO_REQ_TYPE_GET_MASK) {
		switch (bRequest) {
			case AUDIO_REQ_GET_Cur:
				value = selector_unit[selector_index] + 1;
				break;
			case AUDIO_REQ_GET_Min:
				value = 1;
				break;
			case AUDIO_REQ_GET_Max:
				value = num_selections[selector_index];
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
		// FIXME check this is correct order to send the three bytes
		Endpoint_Write_Control_Stream(&value, 1);
		Endpoint_ClearSetupOUT();
		return;
	}
	else {
		if (bRequest == AUDIO_REQ_SET_Cur) {
			/* A request for the current setting of a particular channel's input gain. */
			Endpoint_ClearSetupReceived();
			Endpoint_Read_Control_Stream(&value, 1);
			Endpoint_ClearSetupIN();

			// store the new value (HACK check that we need to subtract 1)
			selector_unit[selector_index] = value - 1;
			
			return;
		}
	}

	// FIXME send NAK?
	SendNAK();
}


void ProcessVolumeRequest(uint8_t bRequest, uint8_t bmRequestType,
		uint8_t entityId, uint8_t channelNumber)
{
	uint8_t buf;
	int16_t value;

	// FIXME add master control: if channelNumber == 0xFF, then get all gain control settings.
	
	// convert the USB channel number into our microphone ADC index
	uint8_t alternate_setting;
	switch (entityId) {
		case FEATURE_UNIT_ID1:
			alternate_setting = 0;
			break;
		case FEATURE_UNIT_ID2:
			alternate_setting = 1;
			break;
		case FEATURE_UNIT_ID4:
			alternate_setting = 2;
			break;
		case FEATURE_UNIT_ID8:
			alternate_setting = 3;
			break;
		default:
			alternate_setting = 0;
	}
	uint8_t microphone_index = GetMicrophoneIndex(channelNumber, alternate_setting);

	if (microphone_index == -1) {
		SendNAK();
		return;
	}

	// find out if its a "get" or a "set" request
	if (bmRequestType & AUDIO_REQ_TYPE_GET_MASK) {
		switch (bRequest) {
			case AUDIO_REQ_GET_Cur:
				if (PreAmps_get(microphone_index, &buf)) {
					// use approximate half-max as fall-back if error occurs
					buf = 0x1f;
				}
				channel_volume[microphone_index] = ConvertByteToVolume(buf);
				value = channel_volume[microphone_index];
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
			/* A request to set a particular channel's input gain. */
			Endpoint_ClearSetupReceived();
			Endpoint_Read_Control_Stream(&value, sizeof(value));
			Endpoint_ClearSetupIN();

			// cache the value
			channel_volume[microphone_index] = value;
			// convert to a digital pot value
			uint8_t pot_value = ConvertVolumeToByte(value);
			// set the pot
			PreAmps_set(microphone_index, pot_value);
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
			/* A request to set the sampling frequency */
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


/** Determine the microphone ADC index (starting at 0) of the specified 
 * channel (starting at 1) in the current configuration */
uint8_t GetMicrophoneIndex(uint8_t channel, uint8_t alternateSetting)
{
	// ensure that it's a valid channel number for that alternate setting
	if (channel < 1 || channel > num_channels[alternateSetting]) {
		return -1;
	}
	
	// convert the USB channel number into our microphone ADC index
	switch (alternateSetting) {
		case 0:
			return selections_1[selector_unit[alternateSetting]][channel - 1];
		case 1:
			return selections_2[selector_unit[alternateSetting]][channel - 1];
		case 2:
			return selections_4[selector_unit[alternateSetting]][channel - 1];
		case 3:
			return selections_8[selector_unit[alternateSetting]][channel - 1];
	}

	return 0;
}


/** Update next_channel array for the interrupt handler.
 * This involves marking muted channels with -1, and the rest
 * with the next enabled channel to sample. */
void UpdateNextChannelArray(uint8_t alternateSetting)
{
	uint8_t current_channels = num_channels[alternateSetting];
	
	for (uint8_t i = 1; i <= current_channels; ++i) {
		next_channel[i-1] = GetMicrophoneIndex(
				i % current_channels + 1, alternateSetting);
 	}
}


void ResetADC(uint8_t alternateSetting)
{
	ADC_ReadSampleAndSetNextAddr(GetMicrophoneIndex(1, alternateSetting));

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
	double feedback_resistance = DIGITAL_POT_RESISTANCE_BASE + byte 
			* (DIGITAL_POT_RESISTANCE_MAX - DIGITAL_POT_RESISTANCE_BASE) / 0xff;
	// convert feedback resistor value into a voltage gain 
	// (positive gain amplifier)
	double voltage_gain = (feedback_resistance / PREAMP_INPUT_RESISTANCE) + 1;
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


/** This puts the given 16bit value on ports a(msb) and c(lsb)
 */
static inline void ShowVal(uint16_t val)
{
	// enable port a & c for debugging
	DDRA = 0xFF;
	DDRC = 0xFF;
	
	PORTA = ((val >> 8) & 0xFF);
	PORTC = (val & 0xFF);
}
