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

#ifndef _DESCRIPTORS_H_
#define _DESCRIPTORS_H_

/* Includes: */
#include "Shared.h"
#include <MyUSB/Drivers/USB/USB.h>
#include <avr/pgmspace.h>


/* Audio Device Class control requests for USB Audio 1.0. */
#define AUDIO_REQ_TYPE_GET_MASK   0x80

#define AUDIO_REQ_GET_Cur    0x81
#define AUDIO_REQ_GET_Min    0x82
#define AUDIO_REQ_GET_Max    0x83
#define AUDIO_REQ_GET_Res    0x84

#define AUDIO_REQ_SET_Cur    0x01


/* Macros: */
#define DTYPE_AudioInterface        0x24
#define DTYPE_AudioEndpoint         0x25

#define DSUBTYPE_Header             0x01
#define DSUBTYPE_InputTerminal      0x02
#define DSUBTYPE_OutputTerminal     0x03
#define DSUBTYPE_FeatureUnit        0x06

#define DSUBTYPE_General            0x01
#define DSUBTYPE_Format             0x02

#define CHANNEL_LEFT_FRONT          (1 << 0)
#define CHANNEL_RIGHT_FRONT         (1 << 1)
#define CHANNEL_CENTER_FRONT        (1 << 2)
#define CHANNEL_LOW_FREQ_ENHANCE    (1 << 3)
#define CHANNEL_LEFT_SURROUND       (1 << 4)
#define CHANNEL_RIGHT_SURROUND      (1 << 5)
#define CHANNEL_LEFT_OF_CENTER      (1 << 6)
#define CHANNEL_RIGHT_OF_CENTER     (1 << 7)
#define CHANNEL_SURROUND            (1 << 8)
#define CHANNEL_SIDE_LEFT           (1 << 9)
#define CHANNEL_SIDE_RIGHT          (1 << 10)
#define CHANNEL_TOP                 (1 << 11)

#define FEATURE_MUTE                (1 << 0)
#define FEATURE_VOLUME              (1 << 1)
#define FEATURE_BASS                (1 << 2)
#define FEATURE_MID                 (1 << 3)
#define FEATURE_TREBLE              (1 << 4)
#define FEATURE_GRAPHIC_EQUALIZER   (1 << 5)
#define FEATURE_AUTOMATIC_GAIN      (1 << 6)
#define FEATURE_DELAY               (1 << 7)
#define FEATURE_BASS_BOOST          (1 << 8)
#define FEATURE_BASS_LOUDNESS       (1 << 9)

#define TERMINAL_UNDEFINED          0x0100
#define TERMINAL_STREAMING          0x0101
#define TERMINAL_VENDOR             0x01FF
#define TERMINAL_IN_UNDEFINED       0x0200
#define TERMINAL_IN_MIC             0x0201
#define TERMINAL_IN_DESKTOP_MIC     0x0202
#define TERMINAL_IN_PERSONAL_MIC    0x0203
#define TERMINAL_IN_OMNIDIR_MIC     0x0204
#define TERMINAL_IN_MIC_ARRAY       0x0205
#define TERMINAL_IN_PROCESSING_MIC  0x0206
#define TERMINAL_IN_OUT_UNDEFINED   0x0300
#define TERMINAL_OUT_SPEAKER        0x0301
#define TERMINAL_OUT_HEADPHONES     0x0302
#define TERMINAL_OUT_HEAD_MOUNTED   0x0303
#define TERMINAL_OUT_DESKTOP        0x0304
#define TERMINAL_OUT_ROOM           0x0305
#define TERMINAL_OUT_COMMUNICATION  0x0306
#define TERMINAL_OUT_LOWFREQ        0x0307

#define SAMPLE_FREQ_LOW_WORD(x) ((uint32_t)x & 0x0000FFFF)
#define SAMPLE_FREQ_HIGH_BYTE(x) (((uint32_t)x >> 16) & 0x000000FF)
#define SAMPLE_FREQ(x) { LowWord: SAMPLE_FREQ_LOW_WORD(x), HighByte: SAMPLE_FREQ_HIGH_BYTE(x)}

// Terminal IDs.
#define INPUT_TERMINAL_ID  0x01
#define FEATURE_UNIT_ID    0x02
#define OUTPUT_TERMINAL_ID 0x03

/* Type Defines: */

// Interface Header Audio Class Descriptor
typedef struct
{
  USB_Descriptor_Header_t   Header;
  uint8_t                   Subtype;

  uint16_t                  ACSpecification;
  uint16_t                  TotalLength;
			
  uint8_t                   InCollection;
  uint8_t                   InterfaceNumbers[1];
} USB_AudioInterface_AC_t;

// Generalised to more than 2 channels.
// This is horrible, but it needs to be statically sized so it can be
// crammed into PROGMEM easily.
#define FEATURE_UNIT_STRUCT(xxxNUM_CHANNELSxxx) typedef struct \
{ \
  USB_Descriptor_Header_t   Header; \
  uint8_t                   Subtype; \
\
  uint8_t                   UnitID; \
  uint8_t                   SourceID; \
\
  uint8_t                   ControlSize; \
  uint8_t                   MasterControls; \
  uint8_t                   ChannelControls[xxxNUM_CHANNELSxxx]; \
\
  uint8_t                   FeatureUnitStrIndex; \
} USB_AudioFeatureUnit ## xxxNUM_CHANNELSxxx ## _t

FEATURE_UNIT_STRUCT(8);
FEATURE_UNIT_STRUCT(4);
FEATURE_UNIT_STRUCT(2);
FEATURE_UNIT_STRUCT(1);

typedef struct
{
  USB_Descriptor_Header_t   Header;
  uint8_t                   Subtype;

  uint8_t                   TerminalID;
  uint16_t                  TerminalType;
  uint8_t                   AssociatedOutputTerminal;

  uint8_t                   TotalChannels;
  uint16_t                  ChannelConfig;

  uint8_t                   ChannelStrIndex;
  uint8_t                   TerminalStrIndex;
} USB_AudioInputTerminal_t;

typedef struct
{
  USB_Descriptor_Header_t   Header;
  uint8_t                   Subtype;

  uint8_t                   TerminalID;
  uint16_t                  TerminalType;
  uint8_t                   AssociatedInputTerminal;

  uint8_t                   SourceID;

  uint8_t                   TerminalStrIndex;
} USB_AudioOutputTerminal_t;

// Audio Stream Interface Descriptor
typedef struct
{
  USB_Descriptor_Header_t   Header;
  uint8_t                   Subtype;

  uint8_t                   TerminalLink;

  uint8_t                   FrameDelay;
  uint16_t                  AudioFormat;
} USB_AudioInterface_AS_t;

typedef struct
{
  uint16_t                  LowWord;
  uint8_t                   HighByte;
} AudioSampleFreq_t;

typedef struct
{
  USB_Descriptor_Header_t   Header;
  uint8_t                   Subtype;

  uint8_t                   FormatType;
  uint8_t                   Channels;

  uint8_t                   SubFrameSize;
  uint8_t                   BitResolution;
  uint8_t                   SampleFrequencyType;

//   AudioSampleFreq_t         SampleFrequencies[1];
  AudioSampleFreq_t         SampleFrequencies[2];
} USB_AudioFormat_t;

typedef struct
{
  USB_Descriptor_Endpoint_t Endpoint;

  uint8_t                   Refresh;
  uint8_t                   SyncEndpointNumber;
} USB_AudioStreamEndpoint_Std_t;

typedef struct
{
  USB_Descriptor_Header_t   Header;
  uint8_t                   Subtype;

  uint8_t                   Attributes;

  uint8_t                   LockDelayUnits;
  uint16_t                  LockDelay;
} USB_AudioStreamEndpoint_Spc_t;	

// Configuration Descriptor
// This is horrible, but it needs to be statically sized so it can be
// crammed into PROGMEM easily.
#define DESCRIPTOR_CONFIGURATION_STRUCT(xxxNUM_CHANNELSxxx) typedef struct \
{ \
  USB_Descriptor_Configuration_Header_t Config; \
  USB_Descriptor_Interface_t            AudioControlInterface; \
  USB_AudioInterface_AC_t               AudioControlInterface_SPC; /* lists terminals, etc. */ \
  USB_AudioInputTerminal_t              InputTerminal; /* from the mics. */ \
  USB_AudioFeatureUnit ## xxxNUM_CHANNELSxxx ## _t               FeatureUnit; /* pre-amp controls */ \
  USB_AudioOutputTerminal_t             OutputTerminal; /* mandatory, not used. */ \
  USB_Descriptor_Interface_t            AudioStreamInterface_Alt0; /* isochronous endpoint */ \
  USB_Descriptor_Interface_t            AudioStreamInterface_Alt1;  /* non-isochronous endpoint, chosen if we lack bandwidth. */ \
  USB_AudioInterface_AS_t               AudioStreamInterface_SPC; /* describes the audio stream */ \
  USB_AudioFormat_t                     AudioFormat; /* format of the audio stream */ \
  USB_AudioStreamEndpoint_Std_t         AudioEndpoint; /* isochronous endpoint */ \
  USB_AudioStreamEndpoint_Spc_t         AudioEndpoint_SPC; /* audio-class specifics of the endpoint */ \
} USB_Descriptor_Configuration ## xxxNUM_CHANNELSxxx ## _t

DESCRIPTOR_CONFIGURATION_STRUCT(8);
DESCRIPTOR_CONFIGURATION_STRUCT(4);
DESCRIPTOR_CONFIGURATION_STRUCT(2);
DESCRIPTOR_CONFIGURATION_STRUCT(1);

#define CONFIGURATION_DESCRIPTOR(xxxCONFIGURATION_NUMBERxxx, xxxNUM_CHANNELSxxx, xxxCHANNEL_FEATURESxxx) \
	{ \
		Config: { \
			Header: { \
				Size: sizeof(USB_Descriptor_Configuration_Header_t), \
				Type: DTYPE_Configuration \
			}, \
			TotalConfigurationSize: sizeof(USB_Descriptor_Configuration ## xxxNUM_CHANNELSxxx ## _t), \
			TotalInterfaces: 2, \
			ConfigurationNumber: xxxCONFIGURATION_NUMBERxxx, \
			ConfigurationStrIndex: NO_DESCRIPTOR_STRING, \
			ConfigAttributes: USB_CONFIG_ATTR_BUSPOWERED, /* just bus-powered. */ \
			MaxPowerConsumption: USB_CONFIG_POWER_MA(100) \
		}, \
		/* mandatory audio control interface. */ \
		AudioControlInterface: { \
			Header: { \
				Size: sizeof(USB_Descriptor_Interface_t), \
				Type: DTYPE_Interface \
			}, \
			InterfaceNumber: 0, \
			AlternateSetting: 0, \
			TotalEndpoints: 0,  /* 0 according to the spec, unless we use interrupts. */ \
			Class: 0x01, \
			SubClass: 0x01, \
			Protocol: 0x00, \
			InterfaceStrIndex: NO_DESCRIPTOR_STRING \
		}, \
\
		/* specifics of the audio control interface */ \
		AudioControlInterface_SPC: { \
			Header: { \
				Size: sizeof(USB_AudioInterface_AC_t), \
				Type: DTYPE_AudioInterface \
			}, \
			Subtype: DSUBTYPE_Header, \
			ACSpecification: VERSION_BCD(01.00), /* follows the audio spec 1.0 */ \
			TotalLength: (sizeof(USB_AudioInterface_AC_t) \
					+ sizeof(USB_AudioInputTerminal_t) \
					+ sizeof(USB_AudioFeatureUnit ## xxxNUM_CHANNELSxxx ## _t) \
					+ sizeof(USB_AudioOutputTerminal_t)), \
			InCollection: 1,  /* 1 streaming interface */ \
			InterfaceNumbers: { 1 },   /* interface 1 is the stream */ \
		}, \
\
		/* We have one audio cluster, specified as an array of microphones */ \
		InputTerminal: { \
			Header: { \
				Size: sizeof(USB_AudioInputTerminal_t), \
				Type: DTYPE_AudioInterface \
			}, \
			Subtype: DSUBTYPE_InputTerminal, \
			TerminalID: INPUT_TERMINAL_ID, \
			TerminalType: TERMINAL_IN_MIC_ARRAY, \
			AssociatedOutputTerminal: 0x00, \
			TotalChannels: xxxNUM_CHANNELSxxx, \
			ChannelConfig: 0,  /* spatial characteristics (0 == unspecified) */ \
			ChannelStrIndex: NO_DESCRIPTOR_STRING, \
			TerminalStrIndex: NO_DESCRIPTOR_STRING \
		}, \
\
		/* Provide access to the pre-amps via a "feature unit". */ \
		FeatureUnit: { \
			Header: { \
				Size: sizeof(USB_AudioFeatureUnit ## xxxNUM_CHANNELSxxx ## _t), \
				Type: DTYPE_AudioInterface \
			}, \
			Subtype: DSUBTYPE_FeatureUnit, \
			UnitID: FEATURE_UNIT_ID, \
			SourceID: INPUT_TERMINAL_ID, \
			ControlSize: 1,  /* the controls are described with one byte. */ \
			MasterControls: 0, /* master, applies to all channels. */ \
			ChannelControls: { \
				xxxCHANNEL_FEATURESxxx \
			},  /* per-channel controls, one entry per channel */ \
			FeatureUnitStrIndex: NO_DESCRIPTOR_STRING \
		}, \
\
		/* mandatory output terminal. */ \
		OutputTerminal: { \
			Header: { \
				Size: sizeof(USB_AudioOutputTerminal_t), \
				Type: DTYPE_AudioInterface \
			}, \
			Subtype: DSUBTYPE_OutputTerminal, \
			TerminalID: OUTPUT_TERMINAL_ID, \
			TerminalType: TERMINAL_STREAMING, \
			AssociatedInputTerminal: 0x00, \
			SourceID: FEATURE_UNIT_ID, /* source from the feature unit */ \
			TerminalStrIndex: NO_DESCRIPTOR_STRING \
		}, \
\
		/* the audio stream interface (non-isochronous) */ \
		/* alternate 0 without endpoint (no audio available to usb host) */ \
		AudioStreamInterface_Alt0: { \
			Header: { \
				Size: sizeof(USB_Descriptor_Interface_t), \
				Type: DTYPE_Interface \
			}, \
			InterfaceNumber: 1, \
			AlternateSetting: 0, \
			TotalEndpoints: 0, \
			Class: 0x01, \
			SubClass: 0x02, \
			Protocol: 0x00, \
			InterfaceStrIndex: NO_DESCRIPTOR_STRING \
		}, \
\
		/* mandatory actual audio stream interface (isochronous) */ \
		/* alternate 1 with actual audio on it. */ \
		AudioStreamInterface_Alt1: { \
			Header: { \
				Size: sizeof(USB_Descriptor_Interface_t), \
				Type: DTYPE_Interface \
			}, \
			InterfaceNumber: 1, \
			AlternateSetting: 1, \
			TotalEndpoints: 1, \
			Class: 0x01, \
			SubClass: 0x02, \
			Protocol: 0x00, \
			InterfaceStrIndex: NO_DESCRIPTOR_STRING \
		}, \
\
		/* audio stream interface specifics */ \
		AudioStreamInterface_SPC: { \
			Header: { \
				Size: sizeof(USB_AudioInterface_AS_t), \
				Type: DTYPE_AudioInterface \
			}, \
			Subtype: DSUBTYPE_General, \
			TerminalLink: OUTPUT_TERMINAL_ID,  /* the stream comes from the output terminal, after the feature unit */ \
			FrameDelay: 1,   /* interface delay (p22 of Audio20 final.pdf) */ \
			AudioFormat: 0x0001  /* 16bit PCM format */ \
		}, \
\
		/* FIXME this is where we say what the samples look like. */ \
		/* 16 bit samples, XX channels, XXXX samples/sec (in Descriptors.h) */ \
		/* Info in the USB Audio Formats document. */ \
		AudioFormat: { \
			Header: { \
				Size: sizeof(USB_AudioFormat_t), \
				Type: DTYPE_AudioInterface \
			}, \
			Subtype: DSUBTYPE_Format, \
			FormatType: 0x01,  /* FORMAT_TYPE_1 */ \
			Channels: xxxNUM_CHANNELSxxx, \
			SubFrameSize: 0x02,  /* 2 bytes per sample */ \
			BitResolution: 0x0C, /* We use 12 bits of the 2 bytes */ \
			SampleFrequencyType: 0, /* continous sampling frequency setting supported */ \
			SampleFrequencies: { \
				SAMPLE_FREQ(LOWEST_AUDIO_SAMPLE_FREQUENCY), \
				SAMPLE_FREQ(HIGHEST_AUDIO_SAMPLE_FREQUENCY) \
			} \
		}, \
\
		/* The actual isochronous audio sample endpoint. */ \
		AudioEndpoint: { \
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
		AudioEndpoint_SPC: { \
			Header: { \
				Size: sizeof(USB_AudioStreamEndpoint_Spc_t), \
				Type: DTYPE_AudioEndpoint \
			}, \
			Subtype: DSUBTYPE_General, \
			Attributes: EP_CS_ATTR_SAMPLE_RATE, \
			LockDelayUnits: 0x02,  /* FIXME reserved value for PCM streams? */ \
			LockDelay: 0x0000  /* 0 for async streams */ \
		} \
	}


/** expose public variables */ 
extern uint8_t num_channels[NUM_CONFIGURATIONS];
extern uint8_t ADC_channels[NUM_CONFIGURATIONS][MAX_AUDIO_CHANNELS];


/** Function Prototypes: */
bool USB_GetDescriptor(const uint16_t wValue, const uint8_t wIndex,
		void** const DescriptorAddress, uint16_t* const DescriptorSize)
				ATTR_WARN_UNUSED_RESULT ATTR_WEAK ATTR_NON_NULL_PTR_ARG(3, 4);

#endif
