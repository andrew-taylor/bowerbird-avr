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

USB_Descriptor_Device_t DeviceDescriptor PROGMEM =
{
 Header:                 {Size: sizeof(USB_Descriptor_Device_t), Type: DTYPE_Device},

 USBSpecification:       VERSION_BCD(01.10),
 Class:                  0x00,
 SubClass:               0x00,
 Protocol:               0x00,

 Endpoint0Size:          8,
		
 VendorID:               0x03EB,
 ProductID:              0x2047,
 ReleaseNumber:          0x0100,

 ManufacturerStrIndex:   0x01,
 ProductStrIndex:        0x02,
 SerialNumStrIndex:      0x03,

 NumberOfConfigurations: 1
};
	
USB_Descriptor_Configuration_t ConfigurationDescriptor PROGMEM =
  { Config:
    { Header:                   { Size: sizeof(USB_Descriptor_Configuration_Header_t),
				  Type: DTYPE_Configuration },
      TotalConfigurationSize:   sizeof(USB_Descriptor_Configuration_t),
      TotalInterfaces:          2,
      ConfigurationNumber:      1,
      ConfigurationStrIndex:    NO_DESCRIPTOR_STRING,
      ConfigAttributes:         USB_CONFIG_ATTR_BUSPOWERED, // just bus-powered.
      MaxPowerConsumption:      USB_CONFIG_POWER_MA(100)
    },

    // mandatory audio control interface.
    AudioControlInterface:
    { Header:                   { Size: sizeof(USB_Descriptor_Interface_t),
				  Type: DTYPE_Interface },
      InterfaceNumber:          0,
      AlternateSetting:         0,
      TotalEndpoints:           0, // 0 according to the spec, unless we use interrupts.
      Class:                    0x01,
      SubClass:                 0x01,
      Protocol:                 0x00,
      InterfaceStrIndex:        NO_DESCRIPTOR_STRING			
    },

    // specifics of the audio control interface
    AudioControlInterface_SPC:
    { Header:                   { Size: sizeof(USB_AudioInterface_AC_t),
				  Type: DTYPE_AudioInterface },
      Subtype:                  DSUBTYPE_Header,
      ACSpecification:          VERSION_BCD(01.00), // follows the audio spec 1.0
      TotalLength:              (sizeof(USB_AudioInterface_AC_t)
				 + sizeof(USB_AudioInputTerminal_t)
				 + sizeof(USB_AudioFeatureUnit_t)
				 + sizeof(USB_AudioOutputTerminal_t)),
      InCollection:             1, // 1 streaming interface
      InterfaceNumbers:         {1}, // interface 1 is the stream
    },

    // We have one audio cluster.
    InputTerminal:
    { Header:                   { Size: sizeof(USB_AudioInputTerminal_t),
				  Type: DTYPE_AudioInterface },
      Subtype:                  DSUBTYPE_InputTerminal,
      TerminalID:               INPUT_TERMINAL_ID,
      TerminalType:             TERMINAL_IN_MIC_ARRAY,
      AssociatedOutputTerminal: 0x00,
      TotalChannels:            AUDIO_CHANNELS,
      ChannelConfig:            0, // spatial characteristics (0 == unspecified)
      ChannelStrIndex:          NO_DESCRIPTOR_STRING,
      TerminalStrIndex:         NO_DESCRIPTOR_STRING
    },

    // Provide access to the pre-amps via a "feature unit".
    FeatureUnit:
    { Header:			{ Size: sizeof(USB_AudioFeatureUnit_t),
				  Type: DTYPE_AudioInterface },
      Subtype:			DSUBTYPE_FeatureUnit,
      UnitID:			FEATURE_UNIT_ID,
      SourceID:			INPUT_TERMINAL_ID,
      ControlSize:              1, // the controls are described with one byte.
      MasterControls:           0, // FEATURE_VOLUME, // FIXME master, applies to all channels.
      ChannelControls:          { FEATURE_VOLUME},//, FEATURE_VOLUME, FEATURE_VOLUME, FEATURE_VOLUME, FEATURE_VOLUME, FEATURE_VOLUME, FEATURE_VOLUME, FEATURE_VOLUME, FEATURE_VOLUME }, // FIXME per-channel controls, one entry per channel
      FeatureUnitStrIndex:	NO_DESCRIPTOR_STRING			
    },

    // mandatory FIXME useless output terminal.
    OutputTerminal:
    { Header:                   { Size: sizeof(USB_AudioOutputTerminal_t),
				  Type: DTYPE_AudioInterface },
      Subtype:                  DSUBTYPE_OutputTerminal,
      TerminalID:               OUTPUT_TERMINAL_ID,
      TerminalType:             TERMINAL_STREAMING,
      AssociatedInputTerminal:  0x00,
      SourceID:                 FEATURE_UNIT_ID, // source from the feature unit?
      TerminalStrIndex:         NO_DESCRIPTOR_STRING			
    },

    // mandatory useless the audio stream interface (non-isochronous)
    // for when the host cannot allocate isochronous bandwidth.
    AudioStreamInterface_Alt0:
    { Header:                   { Size: sizeof(USB_Descriptor_Interface_t),
				  Type: DTYPE_Interface },
      InterfaceNumber:          1,
      AlternateSetting:         0,
      TotalEndpoints:           0,
      Class:                    0x01,
      SubClass:                 0x02,
      Protocol:                 0x00,
      InterfaceStrIndex:        NO_DESCRIPTOR_STRING
    },

    // mandatory actual audio stream interface (isochronous)
    AudioStreamInterface_Alt1:
    { Header:                   { Size: sizeof(USB_Descriptor_Interface_t),
				  Type: DTYPE_Interface },
      InterfaceNumber:          1,
      AlternateSetting:         1,
      TotalEndpoints:           1,
      Class:                    0x01,
      SubClass:                 0x02,
      Protocol:                 0x00,
      InterfaceStrIndex:        NO_DESCRIPTOR_STRING
    },

    // audio stream interface specifics
    AudioStreamInterface_SPC:
    { Header:                   { Size: sizeof(USB_AudioInterface_AS_t),
				  Type: DTYPE_AudioInterface },
      Subtype:                  DSUBTYPE_General,
      TerminalLink:             0x03, // the stream comes from the output terminal, after the feature unit
      FrameDelay:               0, // FIXME ? for syncing amongst other USB audio streams?
      AudioFormat:              0x0001 // PCM format
    },

    // FIXME this is where we say what the samples look like.
    // 16 bit samples, XX channels, XXXX samples/sec (in Descriptors.h)
    // Info in the USB Audio Formats document.
    AudioFormat:
    { Header:                   {Size: sizeof(USB_AudioFormat_t), Type: DTYPE_AudioInterface},
      Subtype:                  DSUBTYPE_Format,
      FormatType:               0x01, // FORMAT_TYPE_1
      Channels:                 AUDIO_CHANNELS,
      SubFrameSize:             0x02, // 2 bytes per sample
      BitResolution:            12, //16, // FIXME We use 12 bits of the 2 bytes
      SampleFrequencyType:      (sizeof(ConfigurationDescriptor.AudioFormat.SampleFrequencies) / sizeof(AudioSampleFreq_t)),
      // Could specify several sampling rates here.
      // FIXME verify how this is computed: per stream, or or per channel?
      SampleFrequencies:        {SAMPLE_FREQ(AUDIO_SAMPLE_FREQUENCY)}
    },

    // The actual isocrohonous audio sample endpoint.
    AudioEndpoint:
    { Endpoint:
      { Header:			{ Size: sizeof(USB_AudioStreamEndpoint_Std_t),
				  Type: DTYPE_Endpoint },
	EndpointAddress:	(ENDPOINT_DESCRIPTOR_DIR_IN | AUDIO_STREAM_EPNUM),
	Attributes:		(EP_TYPE_ISOCHRONOUS | ENDPOINT_ATTR_ASYNC), // | ENDPOINT_USAGE_DATA),
	EndpointSize:		AUDIO_STREAM_EPSIZE, // packet size per polling interval (256 bytes)
	PollingIntervalMS:	1 // USB polling rate. I believe this is fixed.
      },
      Refresh:                  0,
      SyncEndpointNumber:       0
    },

    // specifics of the isochronous endpoint.
    AudioEndpoint_SPC:
    { Header:                   { Size: sizeof(USB_AudioStreamEndpoint_Spc_t),
				  Type: DTYPE_AudioEndpoint },
      Subtype:                  DSUBTYPE_General,
      Attributes:               0x00,
      LockDelayUnits:           0x02, // FIXME reserved value for PCM streams?
      LockDelay:                0x0000 // 0 for async streams
    }
  };

USB_Descriptor_String_t LanguageString PROGMEM =
{
 Header:                 {Size: USB_STRING_LEN(1), Type: DTYPE_String},
 UnicodeString:          {LANGUAGE_ID_ENG}
};

USB_Descriptor_String_t ManufacturerString PROGMEM =
{
 Header:                 {Size: USB_STRING_LEN(12), Type: DTYPE_String},
 UnicodeString:          L"Peter Gammie"
};

USB_Descriptor_String_t ProductString PROGMEM =
{
 Header:                 {Size: USB_STRING_LEN(17), Type: DTYPE_String},
 UnicodeString:          L"8-Mic AVR Adaptor"
};

USB_Descriptor_String_t SerialNumberString PROGMEM =
{
 Header:                 {Size: USB_STRING_LEN(2), Type: DTYPE_String},
 UnicodeString:          L"42"
};

bool USB_GetDescriptor(const uint16_t wValue, const uint8_t wIndex,
                       void** const DescriptorAddress, uint16_t* const DescriptorSize)
{
  void*    Address = NULL;
  uint16_t Size    = 0;

  switch (wValue >> 8) {
  case DTYPE_Device:
    Address = DESCRIPTOR_ADDRESS(DeviceDescriptor);
    Size    = sizeof(USB_Descriptor_Device_t);
    break;
  case DTYPE_Configuration:
    Address = DESCRIPTOR_ADDRESS(ConfigurationDescriptor);
    Size    = sizeof(USB_Descriptor_Configuration_t);
    break;
  case DTYPE_String:
    switch (wValue & 0xFF) {
    case 0x00:
      Address = DESCRIPTOR_ADDRESS(LanguageString);
      Size    = pgm_read_byte(&LanguageString.Header.Size);
      break;
    case 0x01:
      Address = DESCRIPTOR_ADDRESS(ManufacturerString);
      Size    = pgm_read_byte(&ManufacturerString.Header.Size);
      break;
    case 0x02:
      Address = DESCRIPTOR_ADDRESS(ProductString);
      Size    = pgm_read_byte(&ProductString.Header.Size);
      break;
    case 0x03:
      Address = DESCRIPTOR_ADDRESS(SerialNumberString);
      Size    = pgm_read_byte(&SerialNumberString.Header.Size);
      break;
    }
    break;
  }
	
  if (Address != NULL) {
    *DescriptorAddress = Address;
    *DescriptorSize    = Size;

    return true;
  } else {
    return false;
  }
}
