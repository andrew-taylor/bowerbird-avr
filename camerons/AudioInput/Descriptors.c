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

#include "Descriptors.h"

/** an array of ADC channel numbers representing which ADC channels
 * are routed to each USB channel for each configuration. The number
 * of USB channels in each configuration must match the configurations
 * below */
uint8_t num_channels[NUM_CONFIGURATIONS] = { /*8,*/ 4/*, 4, 2, 2, 1 */};
uint8_t ADC_channels[NUM_CONFIGURATIONS][MAX_AUDIO_CHANNELS] = {
//		{ 0, 1, 2, 3, 4, 5, 6, 7 },
		{ 0, 2, 4, 6, 0, 0, 0, 0 },
/*		{ 0, 1, 2, 3, 0, 0, 0, 0 },
		{ 0, 2, 0, 0, 0, 0, 0, 0 },
		{ 0, 1, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }*/ };


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


USB_Descriptor_Configuration_t ConfigurationDescriptors[] PROGMEM = {
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
			ConfigAttributes: USB_CONFIG_ATTR_BUSPOWERED, // just bus-powered.
			MaxPowerConsumption: USB_CONFIG_POWER_MA(100)
		},

		// mandatory audio control interface.
		AudioControlInterface: {
			Header: {
				Size: sizeof(USB_Descriptor_Interface_t),
				Type: DTYPE_Interface
			},
			InterfaceNumber: 0,
			AlternateSetting: 0,
			TotalEndpoints: 0,  // 0 according to the spec, unless we use interrupts.
			Class: 0x01,
			SubClass: 0x01,
			Protocol: 0x00,
			InterfaceStrIndex: NO_DESCRIPTOR_STRING
		},

		// specifics of the audio control interface
		AudioControlInterface_SPC: {
			Header: {
				Size: sizeof(USB_AudioInterface_AC_t),
				Type: DTYPE_AudioInterface
			},
			Subtype: DSUBTYPE_Header,
			ACSpecification: VERSION_BCD(01.00), // follows the audio spec 1.0
			TotalLength: (sizeof(USB_AudioInterface_AC_t)
					+ sizeof(USB_AudioInputTerminal_t)
					+ sizeof(USB_AudioFeatureUnit_t)
					+ sizeof(USB_AudioOutputTerminal_t)),
			InCollection: 1,  // 1 streaming interface
			InterfaceNumbers: { 1 },   // interface 1 is the stream
		},

		// We have one audio cluster, specified as an array of microphones
		InputTerminal: {
			Header: {
				Size: sizeof(USB_AudioInputTerminal_t),
				Type: DTYPE_AudioInterface
			},
			Subtype: DSUBTYPE_InputTerminal,
			TerminalID: INPUT_TERMINAL_ID,
			TerminalType: TERMINAL_IN_MIC_ARRAY,
			AssociatedOutputTerminal: 0x00,
			TotalChannels: 4, // must agree with num_channels[0],
			ChannelConfig: 0,  // spatial characteristics (0 == unspecified)
			ChannelStrIndex: NO_DESCRIPTOR_STRING,
			TerminalStrIndex: NO_DESCRIPTOR_STRING
		},

		// Provide access to the pre-amps via a "feature unit".
		FeatureUnit: {
			Header: {
				Size: sizeof(USB_AudioFeatureUnit_t),
				Type: DTYPE_AudioInterface
			},
			Subtype: DSUBTYPE_FeatureUnit,
			UnitID: FEATURE_UNIT_ID,
			SourceID: INPUT_TERMINAL_ID,
			ControlSize: 1,  // the controls are described with one byte.
			MasterControls: 0, // master, applies to all channels.
			ChannelControls: {
				FEATURE_MUTE | FEATURE_VOLUME,
				FEATURE_MUTE | FEATURE_VOLUME,
				FEATURE_MUTE | FEATURE_VOLUME,
				FEATURE_MUTE | FEATURE_VOLUME,
			},  // per-channel controls, one entry per channel
			FeatureUnitStrIndex: NO_DESCRIPTOR_STRING
		},

		// mandatory output terminal.
		OutputTerminal: {
			Header: {
				Size: sizeof(USB_AudioOutputTerminal_t),
				Type: DTYPE_AudioInterface
			},
			Subtype: DSUBTYPE_OutputTerminal,
			TerminalID: OUTPUT_TERMINAL_ID,
			TerminalType: TERMINAL_STREAMING,
			AssociatedInputTerminal: 0x00,
			SourceID: FEATURE_UNIT_ID, // source from the feature unit
			TerminalStrIndex: NO_DESCRIPTOR_STRING
		},

		// the audio stream interface (non-isochronous)
		// alternate 0 without endpoint (no audio available to usb host)
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

		// mandatory actual audio stream interface (isochronous)
		// alternate 1 with
		AudioStreamInterface_Alt1: {
			Header: {
				Size: sizeof(USB_Descriptor_Interface_t),
				Type: DTYPE_Interface
			},
			InterfaceNumber: 1,
			AlternateSetting: 1,
			TotalEndpoints: 1,
			Class: 0x01,
			SubClass: 0x02,
			Protocol: 0x00,
			InterfaceStrIndex: NO_DESCRIPTOR_STRING
		},

		// audio stream interface specifics
		AudioStreamInterface_SPC: {
			Header: {
				Size: sizeof(USB_AudioInterface_AS_t),
				Type: DTYPE_AudioInterface
			},
			Subtype: DSUBTYPE_General,
			TerminalLink: OUTPUT_TERMINAL_ID,  // the stream comes from the output terminal, after the feature unit
			FrameDelay: 1,   // interface delay (p22 of Audio20 final.pdf)
			AudioFormat: 0x0001  // 16bit PCM format
		},

		// FIXME this is where we say what the samples look like.
		// 16 bit samples, XX channels, XXXX samples/sec (in Descriptors.h)
		// Info in the USB Audio Formats document.
		AudioFormat: {
			Header: {
				Size: sizeof(USB_AudioFormat_t),
				Type: DTYPE_AudioInterface
			},
			Subtype: DSUBTYPE_Format,
			FormatType: 0x01,  // FORMAT_TYPE_1
			Channels: 4, // must agree with num_channels[0],
			SubFrameSize: 0x02,  // 2 bytes per sample
			BitResolution: 0x0C, // We use 12 bits of the 2 bytes
			SampleFrequencyType: 0, // continous sampling frequency setting supported
			SampleFrequencies: {
				SAMPLE_FREQ(LOWEST_AUDIO_SAMPLE_FREQUENCY),
				SAMPLE_FREQ(HIGHEST_AUDIO_SAMPLE_FREQUENCY)
			}
		},

		// The actual isochronous audio sample endpoint.
		AudioEndpoint: {
			Endpoint: {
				Header: {
					Size: sizeof(USB_AudioStreamEndpoint_Std_t),
					Type: DTYPE_Endpoint
				},
				EndpointAddress: (ENDPOINT_DESCRIPTOR_DIR_IN | AUDIO_STREAM_EPNUM),
				Attributes: (EP_TYPE_ISOCHRONOUS | ENDPOINT_ATTR_ASYNC), // | ENDPOINT_USAGE_DATA),
				EndpointSize: AUDIO_STREAM_EPSIZE, // packet size per polling interval (256 bytes)
				PollingIntervalMS: 1 // USB polling rate. I believe this is fixed.
			},
			Refresh: 0,
			SyncEndpointNumber: 0
		},

		// specifics of the isochronous endpoint.
		AudioEndpoint_SPC: {
			Header: {
				Size: sizeof(USB_AudioStreamEndpoint_Spc_t),
				Type: DTYPE_AudioEndpoint
			},
			Subtype: DSUBTYPE_General,
			Attributes: EP_CS_ATTR_SAMPLE_RATE,
			LockDelayUnits: 0x02,  // FIXME reserved value for PCM streams?
			LockDelay: 0x0000  // 0 for async streams
		}
	}/*,
	{
	Config: {
			Header: {
				Size: sizeof(USB_Descriptor_Configuration_Header_t),
				Type: DTYPE_Configuration
			},
			TotalConfigurationSize: sizeof(USB_Descriptor_Configuration_t),
			TotalInterfaces: 2,
			ConfigurationNumber: 2,
			ConfigurationStrIndex: NO_DESCRIPTOR_STRING,
			ConfigAttributes: USB_CONFIG_ATTR_BUSPOWERED, // just bus-powered.
			MaxPowerConsumption: USB_CONFIG_POWER_MA(100)
		},

		// mandatory audio control interface.
		AudioControlInterface: {
			Header: {
				Size: sizeof(USB_Descriptor_Interface_t),
				Type: DTYPE_Interface
			},
			InterfaceNumber: 0,
			AlternateSetting: 0,
			TotalEndpoints: 0,  // 0 according to the spec, unless we use interrupts.
			Class: 0x01,
			SubClass: 0x01,
			Protocol: 0x00,
			InterfaceStrIndex: NO_DESCRIPTOR_STRING
		},

		// specifics of the audio control interface
		AudioControlInterface_SPC: {
			Header: {
				Size: sizeof(USB_AudioInterface_AC_t),
				Type: DTYPE_AudioInterface
			},
			Subtype: DSUBTYPE_Header,
			ACSpecification: VERSION_BCD(01.00), // follows the audio spec 1.0
			TotalLength: (sizeof(USB_AudioInterface_AC_t)
					+ sizeof(USB_AudioInputTerminal_t)
					+ sizeof(USB_AudioFeatureUnit_t)
					+ sizeof(USB_AudioOutputTerminal_t)),
			InCollection: 1,  // 1 streaming interface
			InterfaceNumbers: { 1 },   // interface 1 is the stream
		},

		// We have one audio cluster, specified as an array of microphones
		InputTerminal: {
			Header: {
				Size: sizeof(USB_AudioInputTerminal_t),
				Type: DTYPE_AudioInterface
			},
			Subtype: DSUBTYPE_InputTerminal,
			TerminalID: INPUT_TERMINAL_ID,
			TerminalType: TERMINAL_IN_MIC_ARRAY,
			AssociatedOutputTerminal: 0x00,
			TotalChannels: 1, // must agree with num_channels[1],
			ChannelConfig: 0,  // spatial characteristics (0 == unspecified)
			ChannelStrIndex: NO_DESCRIPTOR_STRING,
			TerminalStrIndex: NO_DESCRIPTOR_STRING
		},

		// Provide access to the pre-amps via a "feature unit".
		FeatureUnit: {
			Header: {
				Size: sizeof(USB_AudioFeatureUnit_t),
				Type: DTYPE_AudioInterface
			},
			Subtype: DSUBTYPE_FeatureUnit,
			UnitID: FEATURE_UNIT_ID,
			SourceID: INPUT_TERMINAL_ID,
			ControlSize: 1,  // the controls are described with one byte.
			MasterControls: 0, // master, applies to all channels.
			ChannelControls: {
				FEATURE_MUTE | FEATURE_VOLUME,
			},  // per-channel controls, one entry per channel
			FeatureUnitStrIndex: NO_DESCRIPTOR_STRING
		},

		// mandatory output terminal.
		OutputTerminal: {
			Header: {
				Size: sizeof(USB_AudioOutputTerminal_t),
				Type: DTYPE_AudioInterface
			},
			Subtype: DSUBTYPE_OutputTerminal,
			TerminalID: OUTPUT_TERMINAL_ID,
			TerminalType: TERMINAL_STREAMING,
			AssociatedInputTerminal: 0x00,
			SourceID: FEATURE_UNIT_ID, // source from the feature unit
			TerminalStrIndex: NO_DESCRIPTOR_STRING
		},

		// the audio stream interface (non-isochronous)
		// alternate 0 without endpoint (no audio available to usb host)
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

		// mandatory actual audio stream interface (isochronous)
		// alternate 1 with
		AudioStreamInterface_Alt1: {
			Header: {
				Size: sizeof(USB_Descriptor_Interface_t),
				Type: DTYPE_Interface
			},
			InterfaceNumber: 1,
			AlternateSetting: 1,
			TotalEndpoints: 1,
			Class: 0x01,
			SubClass: 0x02,
			Protocol: 0x00,
			InterfaceStrIndex: NO_DESCRIPTOR_STRING
		},

		// audio stream interface specifics
		AudioStreamInterface_SPC: {
			Header: {
				Size: sizeof(USB_AudioInterface_AS_t),
				Type: DTYPE_AudioInterface
			},
			Subtype: DSUBTYPE_General,
			TerminalLink: OUTPUT_TERMINAL_ID,  // the stream comes from the output terminal, after the feature unit
			FrameDelay: 1,   // interface delay (p22 of Audio20 final.pdf)
			AudioFormat: 0x0001  // 16bit PCM format
		},

		// FIXME this is where we say what the samples look like.
		// 16 bit samples, XX channels, XXXX samples/sec (in Descriptors.h)
		// Info in the USB Audio Formats document.
		AudioFormat: {
			Header: {
				Size: sizeof(USB_AudioFormat_t),
				Type: DTYPE_AudioInterface
			},
			Subtype: DSUBTYPE_Format,
			FormatType: 0x01,  // FORMAT_TYPE_1
			Channels: 1, // must agree with num_channels[1],
			SubFrameSize: 0x02,  // 2 bytes per sample
			BitResolution: 0x0C, // We use 12 bits of the 2 bytes
			SampleFrequencyType: 0, // continous sampling frequency setting supported
			SampleFrequencies: {
				SAMPLE_FREQ(LOWEST_AUDIO_SAMPLE_FREQUENCY),
				SAMPLE_FREQ(HIGHEST_AUDIO_SAMPLE_FREQUENCY)
			}
		},

		// The actual isochronous audio sample endpoint.
		AudioEndpoint: {
			Endpoint: {
				Header: {
					Size: sizeof(USB_AudioStreamEndpoint_Std_t),
					Type: DTYPE_Endpoint
				},
				EndpointAddress: (ENDPOINT_DESCRIPTOR_DIR_IN | AUDIO_STREAM_EPNUM),
				Attributes: (EP_TYPE_ISOCHRONOUS | ENDPOINT_ATTR_ASYNC), // | ENDPOINT_USAGE_DATA),
				EndpointSize: AUDIO_STREAM_EPSIZE, // packet size per polling interval (256 bytes)
				PollingIntervalMS: 1 // USB polling rate. I believe this is fixed.
			},
			Refresh: 0,
			SyncEndpointNumber: 0
		},

		// specifics of the isochronous endpoint.
		AudioEndpoint_SPC: {
			Header: {
				Size: sizeof(USB_AudioStreamEndpoint_Spc_t),
				Type: DTYPE_AudioEndpoint
			},
			Subtype: DSUBTYPE_General,
			Attributes: EP_CS_ATTR_SAMPLE_RATE,
			LockDelayUnits: 0x02,  // FIXME reserved value for PCM streams?
			LockDelay: 0x0000  // 0 for async streams
		}
	}*/
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
	UnicodeString: L"42"
};

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
			Address = DESCRIPTOR_ADDRESS(ConfigurationDescriptors[index]);
			Size = sizeof(USB_Descriptor_Configuration_t);
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
