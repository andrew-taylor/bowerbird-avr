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
#include <MyUSB/Drivers/USB/USB.h>

#include <avr/pgmspace.h>

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

// FIXME do these work? - volume does
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

#define SAMPLE_FREQ(x)              {LowWord: ((uint32_t)x & 0x00FFFF), HighByte: (((uint32_t)x >> 16) & 0x0000FF)}

#define AUDIO_STREAM_EPNUM          1
#define AUDIO_STREAM_EPSIZE         ENDPOINT_MAX_SIZE

// FIXME This can't get too small, otherwise the 8-bit timer isn't fat enough.
#define AUDIO_SAMPLE_FREQUENCY      48000
// #define AUDIO_SAMPLE_FREQUENCY      16000

#define AUDIO_CHANNELS 2

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
typedef struct
{
  USB_Descriptor_Header_t   Header;
  uint8_t                   Subtype;

  uint8_t                   UnitID;
  uint8_t                   SourceID;

  uint8_t                   ControlSize;
  uint16_t                  MasterControls;
  uint16_t                  ChannelControls[AUDIO_CHANNELS];

  uint8_t                   FeatureUnitStrIndex;  
} USB_AudioFeatureUnit_t;

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

  AudioSampleFreq_t         SampleFrequencies[1];
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
  USB_AudioInterface_AC_t               AudioControlInterface_SPC; // lists terminals, etc.
  USB_AudioInputTerminal_t              InputTerminal; // from the mics.
  USB_AudioFeatureUnit_t                FeatureUnit; // pre-amp controls
  USB_AudioOutputTerminal_t             OutputTerminal; // mandatory, not used.
  USB_Descriptor_Interface_t            AudioStreamInterface_Alt0; // isochronous endpoint
  USB_Descriptor_Interface_t            AudioStreamInterface_Alt1;  // non-isochronous endpoint, chosen if we lack bandwidth.
  USB_AudioInterface_AS_t               AudioStreamInterface_SPC; // describes the audio stream
  USB_AudioFormat_t                     AudioFormat; // format of the audio stream
  USB_AudioStreamEndpoint_Std_t         AudioEndpoint; // isochronous endpoint
  USB_AudioStreamEndpoint_Spc_t         AudioEndpoint_SPC; // audio-class specifics of the endpoint
} USB_Descriptor_Configuration_t;
		
/* Function Prototypes: */
bool USB_GetDescriptor(const uint16_t wValue, const uint8_t wIndex,
		       void** const DescriptorAddress, uint16_t* const DescriptorSize)
  ATTR_WARN_UNUSED_RESULT ATTR_WEAK ATTR_NON_NULL_PTR_ARG(3, 4);

#endif
