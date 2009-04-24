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

#ifndef _AUDIO_OUTPUT_H_
#define _AUDIO_OUTPUT_H_

/* Includes: */
#include <avr/io.h>
#include <avr/wdt.h>

#include "Descriptors.h"

#include <MyUSB/Version.h>                      // Library Version Information
#include <MyUSB/Drivers/USB/USB.h>              // USB Functionality
#include <MyUSB/Drivers/USB/LowLevel/DevChapter9.h>

/* Audio Device Class control requests for USB Audio 1.0. */
#define AUDIO_REQ_TYPE_Get   0xA1
#define AUDIO_REQ_TYPE_Set   0x21

#define AUDIO_REQ_GET_Cur    0x81
#define AUDIO_REQ_GET_Min    0x82
#define AUDIO_REQ_GET_Max    0x83
#define AUDIO_REQ_GET_Res    0x84

#define AUDIO_REQ_SET_Cur    0x01

/* Task Definitions: */
TASK(USB_Audio_Task);

/* Event Handlers: */
HANDLES_EVENT(USB_Connect);
HANDLES_EVENT(USB_Disconnect);
HANDLES_EVENT(USB_ConfigurationChanged);
HANDLES_EVENT(USB_Suspend);
HANDLES_EVENT(USB_UnhandledControlPacket);

#endif
