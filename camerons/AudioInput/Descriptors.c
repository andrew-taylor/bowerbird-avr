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

  * hacked by peteg42 at gmail dot com for andrewt at cse dot unsw dot edu dot au
  * From September 2008.

  * USB is little-endian.

I started off trying to follow the USB 2.0 spec, but the Linux USB
Audio driver really wants to talk 1.0. The specs are not
upwards-compatible it seems to me.

*/

/* some preprocessor directive to simplify/tidy Descriptors */
#define TIMES1(x) x
#define TIMES2(x) x, x
#define TIMES4(x) x, x, x, x
#define TIMES8(x) x, x, x, x, x, x, x, x

#include "Descriptors.h"

/** an array of ADC channel numbers representing which ADC channels
 * are routed to each USB channel for each configuration. The number
 * of USB channels in each configuration must match the configurations
 * below */
uint8_t num_channels[NUM_CONFIGURATIONS] = { 8, 4, 4, 2, 2, 1 };
uint8_t ADC_channels[NUM_CONFIGURATIONS][MAX_AUDIO_CHANNELS] = {
		{ 0, 1, 2, 3, 4, 5, 6, 7 },
		{ 0, 2, 4, 6, 0, 0, 0, 0 },
		{ 0, 1, 2, 3, 0, 0, 0, 0 },
		{ 0, 2, 0, 0, 0, 0, 0, 0 },
		{ 0, 1, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
};


USB_Descriptor_Device_t DeviceDescriptor PROGMEM = {
	Header: {
		Size: sizeof(USB_Descriptor_Device_t),
		Type: DTYPE_Device
	},

	USBSpecification: VERSION_BCD(01.10),
	Class: 0x00,
	SubClass: 0x00,
	Protocol: 0x00,

	Endpoint0Size: 64,
	VendorID: 0x03EB,
	ProductID: 0x2047,
	ReleaseNumber: 0x0100,

	ManufacturerStrIndex: 0x01,
	ProductStrIndex: 0x02,
	SerialNumStrIndex: 0x03,

	NumberOfConfigurations: NUM_CONFIGURATIONS
};


USB_Descriptor_Configuration8_t ConfigurationDescriptor1 PROGMEM =
 	CONFIGURATION_DESCRIPTOR(1, 8, TIMES8(FEATURE_MUTE | FEATURE_VOLUME));
USB_Descriptor_Configuration4_t ConfigurationDescriptor2 PROGMEM =
	CONFIGURATION_DESCRIPTOR(2, 4, TIMES4(FEATURE_MUTE | FEATURE_VOLUME));
USB_Descriptor_Configuration4_t ConfigurationDescriptor3 PROGMEM =
	CONFIGURATION_DESCRIPTOR(3, 4, TIMES4(FEATURE_MUTE | FEATURE_VOLUME));
USB_Descriptor_Configuration2_t ConfigurationDescriptor4 PROGMEM =
	CONFIGURATION_DESCRIPTOR(4, 2, TIMES2(FEATURE_MUTE | FEATURE_VOLUME));
USB_Descriptor_Configuration2_t ConfigurationDescriptor5 PROGMEM =
	CONFIGURATION_DESCRIPTOR(5, 2, TIMES2(FEATURE_MUTE | FEATURE_VOLUME));
USB_Descriptor_Configuration1_t ConfigurationDescriptor6 PROGMEM =
	CONFIGURATION_DESCRIPTOR(6, 1, TIMES1(FEATURE_MUTE | FEATURE_VOLUME));


USB_Descriptor_String_t LanguageString PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(1),
		Type: DTYPE_String
	},
	UnicodeString: {
		LANGUAGE_ID_ENG
	}
};

USB_Descriptor_String_t ManufacturerString PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(17),
		Type: DTYPE_String
	},
	UnicodeString: L"Taylored Products"
};

USB_Descriptor_String_t ProductString PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(17),
		Type: DTYPE_String
	},
	UnicodeString: L"8-Mic AVR Adaptor"
};

USB_Descriptor_String_t SerialNumberString PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(2),
		Type: DTYPE_String
	},
	UnicodeString: L"42"
};


// this just makes things easier
#define CONFIG_DESCRIPTOR_CASE(xxxINDEXxxx) \
				case xxxINDEXxxx: \
					Address = DESCRIPTOR_ADDRESS(ConfigurationDescriptor ## xxxINDEXxxx); \
					Size = sizeof(ConfigurationDescriptor ## xxxINDEXxxx); \
					break

bool USB_GetDescriptor(const uint16_t wValue, const uint8_t wIndex,
		void **const DescriptorAddress, uint16_t * const DescriptorSize)
{
	void *Address = NULL;
	uint16_t Size = 0;

	switch (wValue >> 8) {
		case DTYPE_Device:
			Address = DESCRIPTOR_ADDRESS(DeviceDescriptor);
			Size = sizeof(USB_Descriptor_Device_t);
			break;
		case DTYPE_Configuration: {
			uint8_t index = wValue & 0xFF;
			switch (index) {
				// default case needed
				case 0:
				CONFIG_DESCRIPTOR_CASE(1);
				CONFIG_DESCRIPTOR_CASE(2);
				CONFIG_DESCRIPTOR_CASE(3);
				CONFIG_DESCRIPTOR_CASE(4);
				CONFIG_DESCRIPTOR_CASE(5);
				CONFIG_DESCRIPTOR_CASE(6);
			}
			break;
		}
		case DTYPE_String:
			switch (wValue & 0xFF) {
				case 0x00:
					Address = DESCRIPTOR_ADDRESS(LanguageString);
					Size = pgm_read_byte(&LanguageString.Header.Size);
					break;
				case 0x01:
					Address = DESCRIPTOR_ADDRESS(ManufacturerString);
					Size = pgm_read_byte(&ManufacturerString.Header.Size);
					break;
				case 0x02:
					Address = DESCRIPTOR_ADDRESS(ProductString);
					Size = pgm_read_byte(&ProductString.Header.Size);
					break;
				case 0x03:
					Address = DESCRIPTOR_ADDRESS(SerialNumberString);
					Size = pgm_read_byte(&SerialNumberString.Header.Size);
					break;
			}
			break;
	}

	if (Address != NULL) {
		*DescriptorAddress = Address;
		*DescriptorSize = Size;

		return true;
	}
	else {
		return false;
	}
}
