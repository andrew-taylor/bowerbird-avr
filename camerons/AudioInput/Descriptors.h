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
#define INPUT_TERMINAL_ID1	0x01
#define INPUT_TERMINAL_ID2	0x02
#define INPUT_TERMINAL_ID4	0x03
#define INPUT_TERMINAL_ID8	0x04
#define FEATURE_UNIT_ID1	0x05
#define FEATURE_UNIT_ID2	0x06
#define FEATURE_UNIT_ID4	0x07
#define FEATURE_UNIT_ID8	0x08
#define OUTPUT_TERMINAL_ID1	0x09
#define OUTPUT_TERMINAL_ID2	0x0A
#define OUTPUT_TERMINAL_ID4	0x0B
#define OUTPUT_TERMINAL_ID8	0x0C

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
typedef struct
{
  USB_Descriptor_Configuration_Header_t Config;
  USB_Descriptor_Interface_t            AudioControlInterface;
  USB_AudioInterface_AC_t               AudioControlInterface_SPC; /* lists terminals, etc. */
  USB_AudioInputTerminal_t              InputTerminal1; /* from the mics. */
  USB_AudioInputTerminal_t              InputTerminal2; /* from the mics. */
  USB_AudioInputTerminal_t              InputTerminal4; /* from the mics. */
  USB_AudioInputTerminal_t              InputTerminal8; /* from the mics. */
  USB_AudioFeatureUnit1_t               FeatureUnit1; /* pre-amp controls */
  USB_AudioFeatureUnit2_t               FeatureUnit2; /* pre-amp controls */
  USB_AudioFeatureUnit4_t               FeatureUnit4; /* pre-amp controls */
  USB_AudioFeatureUnit8_t               FeatureUnit8; /* pre-amp controls */
  USB_AudioOutputTerminal_t             OutputTerminal1; /* mandatory, not used. */
  USB_AudioOutputTerminal_t             OutputTerminal2; /* mandatory, not used. */
  USB_AudioOutputTerminal_t             OutputTerminal4; /* mandatory, not used. */
  USB_AudioOutputTerminal_t             OutputTerminal8; /* mandatory, not used. */
  USB_Descriptor_Interface_t            AudioStreamInterface_Alt0; /* isochronous endpoint */
  USB_Descriptor_Interface_t            AudioStreamInterface_Alt1;  /* non-isochronous endpoint, chosen if we lack bandwidth. */
  USB_AudioInterface_AS_t               AudioStreamInterface_SPC1; /* describes the audio stream */
  USB_AudioFormat_t                     AudioFormat1; /* format of the audio stream */
  USB_AudioStreamEndpoint_Std_t         AudioEndpoint1; /* isochronous endpoint */
  USB_AudioStreamEndpoint_Spc_t         AudioEndpoint_SPC1; /* audio-class specifics of the endpoint */
  USB_Descriptor_Interface_t            AudioStreamInterface_Alt2;  /* non-isochronous endpoint, chosen if we lack bandwidth. */
  USB_AudioInterface_AS_t               AudioStreamInterface_SPC2; /* describes the audio stream */
  USB_AudioFormat_t                     AudioFormat2; /* format of the audio stream */
  USB_AudioStreamEndpoint_Std_t         AudioEndpoint2; /* isochronous endpoint */
  USB_AudioStreamEndpoint_Spc_t         AudioEndpoint_SPC2; /* audio-class specifics of the endpoint */
  USB_Descriptor_Interface_t            AudioStreamInterface_Alt3;  /* non-isochronous endpoint, chosen if we lack bandwidth. */
  USB_AudioInterface_AS_t               AudioStreamInterface_SPC3; /* describes the audio stream */
  USB_AudioFormat_t                     AudioFormat3; /* format of the audio stream */
  USB_AudioStreamEndpoint_Std_t         AudioEndpoint3; /* isochronous endpoint */
  USB_AudioStreamEndpoint_Spc_t         AudioEndpoint_SPC3; /* audio-class specifics of the endpoint */
  USB_Descriptor_Interface_t            AudioStreamInterface_Alt4;  /* non-isochronous endpoint, chosen if we lack bandwidth. */
  USB_AudioInterface_AS_t               AudioStreamInterface_SPC4; /* describes the audio stream */
  USB_AudioFormat_t                     AudioFormat4; /* format of the audio stream */
  USB_AudioStreamEndpoint_Std_t         AudioEndpoint4; /* isochronous endpoint */
  USB_AudioStreamEndpoint_Spc_t         AudioEndpoint_SPC4; /* audio-class specifics of the endpoint */
  USB_Descriptor_Interface_t            AudioStreamInterface_Alt5;  /* non-isochronous endpoint, chosen if we lack bandwidth. */
  USB_AudioInterface_AS_t               AudioStreamInterface_SPC5; /* describes the audio stream */
  USB_AudioFormat_t                     AudioFormat5; /* format of the audio stream */
  USB_AudioStreamEndpoint_Std_t         AudioEndpoint5; /* isochronous endpoint */
  USB_AudioStreamEndpoint_Spc_t         AudioEndpoint_SPC5; /* audio-class specifics of the endpoint */
  USB_Descriptor_Interface_t            AudioStreamInterface_Alt6;  /* non-isochronous endpoint, chosen if we lack bandwidth. */
  USB_AudioInterface_AS_t               AudioStreamInterface_SPC6; /* describes the audio stream */
  USB_AudioFormat_t                     AudioFormat6; /* format of the audio stream */
  USB_AudioStreamEndpoint_Std_t         AudioEndpoint6; /* isochronous endpoint */
  USB_AudioStreamEndpoint_Spc_t         AudioEndpoint_SPC6; /* audio-class specifics of the endpoint */
} USB_Descriptor_Configuration_t;


/** expose public variables */ 
extern uint8_t num_channels[NUM_ALTERNATE_SETTINGS];
extern uint8_t ADC_channels[NUM_ALTERNATE_SETTINGS][MAX_AUDIO_CHANNELS];


/** Function Prototypes: */
bool USB_GetDescriptor(const uint16_t wValue, const uint8_t wIndex,
		void** const DescriptorAddress, uint16_t* const DescriptorSize)
				ATTR_WARN_UNUSED_RESULT ATTR_WEAK ATTR_NON_NULL_PTR_ARG(3, 4);

#endif
