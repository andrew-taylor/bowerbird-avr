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

#define INPUT_TERMINAL(xxxIDxxx, xxxNUM_CHANNELSxxx) { \
		Header: { \
			Size: sizeof(USB_AudioInputTerminal_t), \
			Type: DTYPE_AudioInterface \
		}, \
		Subtype: DSUBTYPE_InputTerminal, \
		TerminalID: INPUT_TERMINAL_ID ## xxxIDxxx, \
		TerminalType: TERMINAL_IN_MIC_ARRAY, \
		AssociatedOutputTerminal: 0, \
		TotalChannels: xxxNUM_CHANNELSxxx, \
		ChannelConfig: 0,  /* spatial characteristics (0 == unspecified) */ \
		ChannelStrIndex: NO_DESCRIPTOR_STRING, \
		TerminalStrIndex: INPUT_ID ## xxxIDxxx ## _STRING_INDEX \
	}

/* first argument is the number of inputs, subsequent args are the input terminal ids */
#define SELECTOR_UNIT(xxxNUM_CHANNELSxxx, xxxNUM_INPUTSxxx, ...) { \
		Header: { \
			Size: sizeof(USB_AudioSelectorUnit ## xxxNUM_INPUTSxxx ##_t), \
			Type: DTYPE_AudioInterface \
		}, \
		Subtype: DSUBTYPE_SelectorUnit, \
		UnitID: SELECTOR_UNIT_ID ## xxxNUM_CHANNELSxxx, \
		NumInputs: xxxNUM_INPUTSxxx, \
		SourceIds: { __VA_ARGS__ }, \
		UnitStrIndex: SELECTOR_ID ## xxxNUM_CHANNELSxxx ## _STRING_INDEX \
	}

#define FEATURE_UNIT(xxxNUM_CHANNELSxxx, xxxSOURCE_IDxxx) { \
		Header: { \
			Size: sizeof(USB_AudioFeatureUnit ## xxxNUM_CHANNELSxxx ##_t), \
			Type: DTYPE_AudioInterface \
		}, \
		Subtype: DSUBTYPE_FeatureUnit, \
		UnitID: FEATURE_UNIT_ID ## xxxNUM_CHANNELSxxx, \
		SourceID: xxxSOURCE_IDxxx, \
		ControlSize: 1,  /* the controls are described with one byte. */ \
		MasterControls: 0, /* master, applies to all channels. */ \
		ChannelControls: { TIMES ## xxxNUM_CHANNELSxxx(FEATURE_VOLUME) },  /* per-channel controls, one entry per channel */ \
		UnitStrIndex: FEATURE_ID ## xxxNUM_CHANNELSxxx ## _STRING_INDEX \
	}

#define OUTPUT_TERMINAL(xxxNUM_CHANNELSxxx) { \
		Header: { \
			Size: sizeof(USB_AudioOutputTerminal_t), \
			Type: DTYPE_AudioInterface \
		}, \
		Subtype: DSUBTYPE_OutputTerminal, \
		TerminalID: OUTPUT_TERMINAL_ID ## xxxNUM_CHANNELSxxx, \
		TerminalType: TERMINAL_STREAMING, \
		AssociatedInputTerminal: 0, \
		SourceID: FEATURE_UNIT_ID ## xxxNUM_CHANNELSxxx, /* source from the feature unit */ \
		TerminalStrIndex: NO_DESCRIPTOR_STRING \
	}

#define AUDIO_STREAMING_INTERFACE(xxxALTERNATE_SETTING_NUMBERxxx, xxxNUM_CHANNELSxxx, xxxBYTES_PER_SAMPLExxx, xxxBITS_PER_SAMPLExxx) \
	AudioStreamInterface_Alt ## xxxALTERNATE_SETTING_NUMBERxxx: \
	{ \
		Header: { \
			Size: sizeof(USB_Descriptor_Interface_t), \
			Type: DTYPE_Interface \
		}, \
		InterfaceNumber: 1, \
		AlternateSetting: xxxALTERNATE_SETTING_NUMBERxxx, \
		TotalEndpoints: 1, \
		Class: 0x01, \
		SubClass: 0x02, \
		Protocol: 0x00, \
		InterfaceStrIndex: NO_DESCRIPTOR_STRING \
	}, \
 \
	/* audio stream interface specifics */ \
	AudioStreamInterface_SPC ## xxxALTERNATE_SETTING_NUMBERxxx: { \
		Header: { \
			Size: sizeof(USB_AudioInterface_AS_t), \
			Type: DTYPE_AudioInterface \
		}, \
		Subtype: DSUBTYPE_General, \
		TerminalLink: OUTPUT_TERMINAL_ID ## xxxNUM_CHANNELSxxx,  /* the stream comes from the output terminal, after the feature unit */ \
		FrameDelay: 1,   /* interface delay (p22 of Audio20 final.pdf) */ \
		AudioFormat: 0x0001  /* 16bit PCM format */ \
	}, \
 \
	/* FIXME this is where we say what the samples look like. */ \
	/* 16 bit samples, XX channels, XXXX samples/sec (in Descriptors.h) */ \
	/* Info in the USB Audio Formats document. */ \
	AudioFormat ## xxxALTERNATE_SETTING_NUMBERxxx: { \
		Header: { \
			Size: sizeof(USB_AudioFormat_t), \
			Type: DTYPE_AudioInterface \
		}, \
		Subtype: DSUBTYPE_Format, \
		FormatType: 0x01,  /* FORMAT_TYPE_1 */ \
		Channels:  xxxNUM_CHANNELSxxx, \
		SubFrameSize: xxxBYTES_PER_SAMPLExxx,  /* bytes per sample */ \
		BitResolution: xxxBITS_PER_SAMPLExxx, /* how many bits of the bytes are used */ \
		SampleFrequencyType: 0, /* continous sampling frequency setting supported */ \
		SampleFrequencies: { \
			SAMPLE_FREQ(LOWEST_AUDIO_SAMPLE_FREQUENCY), \
			SAMPLE_FREQ(HIGHEST_AUDIO_SAMPLE_FREQUENCY) \
		} \
	}, \
 \
	/* The actual isochronous audio sample endpoint. */ \
	AudioEndpoint ## xxxALTERNATE_SETTING_NUMBERxxx: { \
		Endpoint: { \
			Header: { \
				Size: sizeof(USB_AudioStreamEndpoint_Std_t), \
				Type: DTYPE_Endpoint \
			}, \
			EndpointAddress: (ENDPOINT_DESCRIPTOR_DIR_IN | AUDIO_STREAM_EPNUM), \
			Attributes: (EP_TYPE_ISOCHRONOUS | ENDPOINT_ATTR_ASYNC), /* | ENDPOINT_USAGE_DATA), */ \
			EndpointSize: AUDIO_STREAM_EPSIZE, /* packet size per polling interval (256 bytes) */ \
			PollingIntervalMS: 1 /* USB polling rate. I believe this is fixed. */ \
		}, \
		Refresh: 0, \
		SyncEndpointNumber: 0 \
	}, \
 \
	/* specifics of the isochronous endpoint. */ \
	AudioEndpoint_SPC ## xxxALTERNATE_SETTING_NUMBERxxx: { \
		Header: { \
			Size: sizeof(USB_AudioStreamEndpoint_Spc_t), \
			Type: DTYPE_AudioEndpoint \
		}, \
		Subtype: DSUBTYPE_General, \
		Attributes: EP_CS_ATTR_SAMPLE_RATE, \
		LockDelayUnits: 0x02,  /* FIXME reserved value for PCM streams? */ \
		LockDelay: 0x0000  /* 0 for async streams */ \
	}


#include "Descriptors.h"


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

	ManufacturerStrIndex: MANUFACTURER_STRING_INDEX,
	ProductStrIndex: PRODUCT_STRING_INDEX,
	SerialNumStrIndex: SERIAL_NUMBER_STRING_INDEX,

	NumberOfConfigurations: 1
};


USB_Descriptor_Configuration_t ConfigurationDescriptor PROGMEM =
{
	Config: {
		Header: {
			Size: sizeof(USB_Descriptor_Configuration_Header_t),
			Type: DTYPE_Configuration
		},
		TotalConfigurationSize: sizeof(USB_Descriptor_Configuration_t),
		TotalInterfaces: 2,
		ConfigurationNumber: 1,
		ConfigurationStrIndex: NO_DESCRIPTOR_STRING,
		ConfigAttributes: USB_CONFIG_ATTR_BUSPOWERED, /* just bus-powered. */
		MaxPowerConsumption: USB_CONFIG_POWER_MA(100)
	},
	/* mandatory audio control interface. */
	AudioControlInterface: {
		Header: {
			Size: sizeof(USB_Descriptor_Interface_t),
			Type: DTYPE_Interface
		},
		InterfaceNumber: 0,
		AlternateSetting: 0,
		TotalEndpoints: 0,  /* 0 according to the spec, unless we use interrupts. */
		Class: 0x01,
		SubClass: 0x01,
		Protocol: 0x00,
		InterfaceStrIndex: NO_DESCRIPTOR_STRING
	},

	/* specifics of the audio control interface */
	AudioControlInterface_SPC: {
		Header: {
			Size: sizeof(USB_AudioInterface_AC_t),
			Type: DTYPE_AudioInterface
		},
		Subtype: DSUBTYPE_Header,
		ACSpecification: VERSION_BCD(01.00), /* follows the audio spec 1.0 */
		TotalLength: (sizeof(USB_AudioInterface_AC_t)
				+ 4 * sizeof(USB_AudioInputTerminal_t)
				+ sizeof(USB_AudioSelectorUnit4_t)
				+ sizeof(USB_AudioFeatureUnit1_t)
				+ sizeof(USB_AudioFeatureUnit2_t)
				+ sizeof(USB_AudioFeatureUnit4_t)
				+ sizeof(USB_AudioFeatureUnit8_t)
				+ 4 * sizeof(USB_AudioOutputTerminal_t)),
		InCollection: 1,  /* 1 streaming interface */
		InterfaceNumbers: { 1 },   /* interface 1 is the stream */
	},

	/* We have one audio cluster, specified as an array of microphones */
	InputTerminal1_1: INPUT_TERMINAL(1_1, 1),
	InputTerminal1_3: INPUT_TERMINAL(1_3, 1),
	InputTerminal1_5: INPUT_TERMINAL(1_5, 1),
	InputTerminal1_7: INPUT_TERMINAL(1_7, 1),
	InputTerminal2_13: INPUT_TERMINAL(2_13, 2),
	InputTerminal2_35: INPUT_TERMINAL(2_35, 2),
	InputTerminal2_57: INPUT_TERMINAL(2_57, 2),
	InputTerminal2_71: INPUT_TERMINAL(2_71, 2),
	InputTerminal4_1357: INPUT_TERMINAL(4_1357, 4),
	InputTerminal4_1234: INPUT_TERMINAL(4_1234, 4),
	InputTerminal4_3456: INPUT_TERMINAL(4_3456, 4),
	InputTerminal4_5678: INPUT_TERMINAL(4_5678, 4),
	InputTerminal4_7812: INPUT_TERMINAL(4_7812, 4),
	InputTerminal8: INPUT_TERMINAL(8, 8),

	SelectorUnit1: SELECTOR_UNIT(1, 4, INPUT_TERMINAL_ID1_1,
			INPUT_TERMINAL_ID1_3, INPUT_TERMINAL_ID1_5, INPUT_TERMINAL_ID1_7),
	SelectorUnit2: SELECTOR_UNIT(2, 4, INPUT_TERMINAL_ID2_13,
			INPUT_TERMINAL_ID2_35, INPUT_TERMINAL_ID2_57,
			INPUT_TERMINAL_ID2_71),
	SelectorUnit4: SELECTOR_UNIT(4, 5, INPUT_TERMINAL_ID4_1357,
			INPUT_TERMINAL_ID4_1234, INPUT_TERMINAL_ID4_3456,
			INPUT_TERMINAL_ID4_5678, INPUT_TERMINAL_ID4_7812),

	/* Provide access to the pre-amps via a "feature unit". */
	FeatureUnit1: FEATURE_UNIT(1, SELECTOR_UNIT_ID1),
	FeatureUnit2: FEATURE_UNIT(2, SELECTOR_UNIT_ID2),
	FeatureUnit4: FEATURE_UNIT(4, SELECTOR_UNIT_ID4),
	FeatureUnit8: FEATURE_UNIT(8, INPUT_TERMINAL_ID8),

	/* mandatory output terminal. */
	OutputTerminal1: OUTPUT_TERMINAL(1),
	OutputTerminal2: OUTPUT_TERMINAL(2),
	OutputTerminal4: OUTPUT_TERMINAL(4),
	OutputTerminal8: OUTPUT_TERMINAL(8),

	/* the audio stream interface (non-isochronous) */
	/* alternate 0 without endpoint (no audio available to usb host) */
	AudioStreamInterface_Alt0: {
		Header: {
			Size: sizeof(USB_Descriptor_Interface_t),
			Type: DTYPE_Interface
		},
		InterfaceNumber: 1,
		AlternateSetting: 0,
		TotalEndpoints: 0,
		Class: 0x01,
		SubClass: 0x02,
		Protocol: 0x00,
		InterfaceStrIndex: NO_DESCRIPTOR_STRING
	},

	/* mandatory actual audio stream interface (isochronous) */
	/* alternates with actual audio on them. */
	AUDIO_STREAMING_INTERFACE(1, 1, 1, 8),
	AUDIO_STREAMING_INTERFACE(2, 2, 2, 12),
	AUDIO_STREAMING_INTERFACE(3, 4, 2, 12),
	AUDIO_STREAMING_INTERFACE(4, 8, 2, 12)
};


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
	UnicodeString: L"43"
};

USB_Descriptor_String_t InputTerminalString1_1 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(6),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 1a"
};

USB_Descriptor_String_t InputTerminalString1_3 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(6),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 2a"
};

USB_Descriptor_String_t InputTerminalString1_5 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(6),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 3a"
};

USB_Descriptor_String_t InputTerminalString1_7 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(6),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 4a"
};

USB_Descriptor_String_t InputTerminalString2_13 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(9),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 1a+2a"
};

USB_Descriptor_String_t InputTerminalString2_35 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(9),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 2a+3a"
};

USB_Descriptor_String_t InputTerminalString2_57 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(9),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 3a+4a"
};

USB_Descriptor_String_t InputTerminalString2_71 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(9),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 4a+1a"
};

USB_Descriptor_String_t InputTerminalString4_1357 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(15),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 1a+2a+3a+4a"
};

USB_Descriptor_String_t InputTerminalString4_1234 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(15),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 1a+1b+2a+2b"
};

USB_Descriptor_String_t InputTerminalString4_3456 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(15),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 2a+2b+3a+3b"
};

USB_Descriptor_String_t InputTerminalString4_5678 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(15),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 3a+3b+4a+4b"
};

USB_Descriptor_String_t InputTerminalString4_7812 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(15),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 4a+4b+1a+1b"
};

USB_Descriptor_String_t InputTerminalString8 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(9),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 1a-4b"
};

USB_Descriptor_String_t SelectorUnitString1 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(22),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 1 Channel Selector"
};

USB_Descriptor_String_t SelectorUnitString2 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(22),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 2 Channel Selector"
};

USB_Descriptor_String_t SelectorUnitString4 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(22),
		Type: DTYPE_String
	},
	UnicodeString: L"Mic 4 Channel Selector"
};

USB_Descriptor_String_t FeatureUnitString1 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(9),
		Type: DTYPE_String
	},
	UnicodeString: L"1 Channel"
};

USB_Descriptor_String_t FeatureUnitString2 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(10),
		Type: DTYPE_String
	},
	UnicodeString: L"2 Channels"
};

USB_Descriptor_String_t FeatureUnitString4 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(10),
		Type: DTYPE_String
	},
	UnicodeString: L"4 Channels"
};

USB_Descriptor_String_t FeatureUnitString8 PROGMEM = {
	Header: {
		Size: USB_STRING_LEN(10),
		Type: DTYPE_String
	},
	UnicodeString: L"8 Channels"
};


/** these tidy up the handling of descriptor strings in the switch statement below */
#define CASE_INPUT_STRING_ID(xxxIDxxx) case INPUT_ID ## xxxIDxxx ## _STRING_INDEX: \
					Address = DESCRIPTOR_ADDRESS(InputTerminalString ## xxxIDxxx); \
					Size = pgm_read_byte(&InputTerminalString ## xxxIDxxx.Header.Size); \
					break
#define CASE_SELECTOR_STRING_ID(xxxIDxxx) case SELECTOR_ID ## xxxIDxxx ## _STRING_INDEX: \
					Address = DESCRIPTOR_ADDRESS(SelectorUnitString ## xxxIDxxx); \
					Size = pgm_read_byte(&SelectorUnitString ## xxxIDxxx.Header.Size); \
					break
#define CASE_FEATURE_STRING_ID(xxxIDxxx) case FEATURE_ID ## xxxIDxxx ## _STRING_INDEX: \
					Address = DESCRIPTOR_ADDRESS(FeatureUnitString ## xxxIDxxx); \
					Size = pgm_read_byte(&FeatureUnitString ## xxxIDxxx.Header.Size); \
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
		case DTYPE_Configuration:
			/* only one configuration */
			Address = DESCRIPTOR_ADDRESS(ConfigurationDescriptor);
			Size = sizeof(ConfigurationDescriptor);
			break;
		case DTYPE_String:
			switch (wValue & 0xFF) {
				case 0x00:
					Address = DESCRIPTOR_ADDRESS(LanguageString);
					Size = pgm_read_byte(&LanguageString.Header.Size);
					break;
				case MANUFACTURER_STRING_INDEX:
					Address = DESCRIPTOR_ADDRESS(ManufacturerString);
					Size = pgm_read_byte(&ManufacturerString.Header.Size);
					break;
				case PRODUCT_STRING_INDEX:
					Address = DESCRIPTOR_ADDRESS(ProductString);
					Size = pgm_read_byte(&ProductString.Header.Size);
					break;
				case SERIAL_NUMBER_STRING_INDEX:
					Address = DESCRIPTOR_ADDRESS(SerialNumberString);
					Size = pgm_read_byte(&SerialNumberString.Header.Size);
					break;
				CASE_INPUT_STRING_ID(1_1);
				CASE_INPUT_STRING_ID(1_3);
				CASE_INPUT_STRING_ID(1_5);
				CASE_INPUT_STRING_ID(1_7);
				CASE_INPUT_STRING_ID(2_13);
				CASE_INPUT_STRING_ID(2_35);
				CASE_INPUT_STRING_ID(2_57);
				CASE_INPUT_STRING_ID(2_71);
				CASE_INPUT_STRING_ID(4_1357);
				CASE_INPUT_STRING_ID(4_1234);
				CASE_INPUT_STRING_ID(4_3456);
				CASE_INPUT_STRING_ID(4_5678);
				CASE_INPUT_STRING_ID(4_7812);
				CASE_INPUT_STRING_ID(8);
				CASE_SELECTOR_STRING_ID(1);
				CASE_SELECTOR_STRING_ID(2);
				CASE_SELECTOR_STRING_ID(4);
				CASE_FEATURE_STRING_ID(1);
				CASE_FEATURE_STRING_ID(2);
				CASE_FEATURE_STRING_ID(4);
				CASE_FEATURE_STRING_ID(8);
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
