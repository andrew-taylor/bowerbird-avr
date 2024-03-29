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

#include "USBMode.h"

#include "LowLevel.h"

#if (!defined(USB_HOST_ONLY) && !defined(USB_DEVICE_ONLY))
volatile uint8_t USB_CurrentMode = USB_MODE_NONE;
#endif

#if !defined(USE_STATIC_OPTIONS)
volatile uint8_t USB_Options;
#endif

void USB_Init(
               #if defined(USB_CAN_BE_BOTH)
               const uint8_t Mode
               #endif

               #if (defined(USB_CAN_BE_BOTH) && !defined(USE_STATIC_OPTIONS))
               ,
               #elif (!defined(USB_CAN_BE_BOTH) && defined(USE_STATIC_OPTIONS))
               void
               #endif
			   
               #if !defined(USE_STATIC_OPTIONS)
               const uint8_t Options
               #endif
               )
{
	if (USB_IsInitialized)
	  USB_ShutDown();

	#if (!defined(USB_HOST_ONLY) && !defined(USB_DEVICE_ONLY))
	USB_CurrentMode = Mode;
	#endif
	
	#if defined(USB_DEVICE_ONLY)
		#if defined(USB_FULL_CONTROLLER)
		UHWCON |= (1 << UIMOD);
		#endif
	#elif defined(USB_HOST_ONLY)
	UHWCON &= ~(1 << UIMOD);
	#else
	if (Mode == USB_MODE_NONE)
	{
		RAISE_EVENT(USB_PowerOnFail, POWERON_ERROR_NoUSBModeSpecified);
		return;
	}
	else if (Mode == USB_MODE_UID)
	{
		UHWCON |=  (1 << UIDE);

		USB_INT_Clear(USB_INT_IDTI);
		USB_INT_Enable(USB_INT_IDTI);
		
		USB_CurrentMode = USB_GetUSBModeFromUID();
	}
	else if (Mode == USB_MODE_DEVICE)
	{
		UHWCON |=  (1 << UIMOD);
	}
	else if (Mode == USB_MODE_HOST)			
	{
		UHWCON &= ~(1 << UIMOD);
	}
	#endif

	#if defined(USB_CAN_BE_BOTH)
	USB_InitTaskPointer();
	#else
	USB_IsInitialized = true;
	#endif

	#if defined(USB_CAN_BE_HOST)
	USB_ControlPipeSize = PIPE_CONTROLPIPE_DEFAULT_SIZE;
	#endif
	
	#if defined(USB_FULL_CONTROLLER) || defined(USB_MODIFIED_FULL_CONTROLLER)
	USB_OTGPAD_On();
	#endif
	
	#if !defined(USE_STATIC_OPTIONS)
	USB_Options = Options;
	#endif

	#if defined(USB_DEVICE_ONLY)
		#if defined(USB_FULL_CONTROLLER) || defined(USB_MODIFIED_FULL_CONTROLLER)
		USB_INT_Enable(USB_INT_VBUS);
		#else
		USB_SetupInterface();
		#endif
	#elif defined(USB_HOST_ONLY)
	USB_SetupInterface();
	#else
	if (USB_CurrentMode == USB_MODE_DEVICE)
	  USB_INT_Enable(USB_INT_VBUS);
	else
	  USB_SetupInterface();
	#endif
	
	sei();
}

void USB_ShutDown(void)
{
	if (USB_IsConnected)
	  RAISE_EVENT(USB_Disconnect);

	USB_INT_DisableAllInterrupts();
	USB_INT_ClearAllInterrupts();

	USB_IsConnected         = false;
	USB_IsInitialized       = false;

	#if defined(USB_CAN_BE_HOST)
	USB_HostState           = HOST_STATE_Unattached;
	#endif

	#if defined(USB_CAN_BE_DEVICE)
	USB_ConfigurationNumber = 0;
	#endif

	#if (!defined(USB_HOST_ONLY) && !defined(USB_DEVICE_ONLY))
	USB_CurrentMode = USB_MODE_NONE;
	#endif
	
	#if !defined(USE_STATIC_OPTIONS)
	USB_Options     = 0;
	#endif

	USB_Interface_Disable();
	USB_PLL_Off();
	
	#if defined(USB_FULL_CONTROLLER) || defined(USB_MODIFIED_FULL_CONTROLLER)
	USB_OTGPAD_Off();
	#endif

	#if defined(USB_CAN_BE_BOTH)
	UHWCON &= ~(1 << UIDE);
	#endif
}

void USB_SetupInterface(void)
{
	USB_INT_DisableAllInterrupts();
	USB_INT_ClearAllInterrupts();

	USB_IsConnected         = false;
	
	#if defined(USB_CAN_BE_BOTH)
	USB_IsInitialized       = false;
	#endif

	#if defined(USB_CAN_BE_HOST)
	USB_HostState           = HOST_STATE_Unattached;
	#endif

	#if defined(USB_CAN_BE_DEVICE)
	USB_ConfigurationNumber  = 0;
	USB_RemoteWakeupEnabled  = false;
	USB_CurrentlySelfPowered = false;
	#endif
	
	#if defined(USB_MODIFIED_FULL_CONTROLLER) && !defined(MANUAL_PLL_CONTROL)
	PLLFRQ = ((1 << PLLUSB) | (1 << PDIV3) | (1 << PDIV1));
	#endif
	
	#if !defined(MANUAL_PLL_CONTROL)
	USB_PLL_On();
	#endif

	while (!(USB_PLL_IsReady()));
		
	USB_Interface_Reset();
	
	#if defined(USB_CAN_BE_BOTH)
	if (UHWCON & (1 << UIDE))
	{
		USB_INT_Clear(USB_INT_IDTI);
		USB_INT_Enable(USB_INT_IDTI);
		USB_CurrentMode = USB_GetUSBModeFromUID();
	}
	#elif defined(USB_DEVICE_ONLY)
		#if defined(USB_FULL_CONTROLLER) || defined(USB_MODIFIED_FULL_CONTROLLER)
		USB_INT_Enable(USB_INT_VBUS);
		#endif
	#endif
		
	if (!(USB_Options & USB_OPT_REG_DISABLED))
	  USB_REG_On();
	
	USB_CLK_Unfreeze();

	#if (defined(USB_CAN_BE_DEVICE) && (defined(USB_FULL_CONTROLLER) || defined(USB_MODIFIED_FULL_CONTROLLER)))
	if (USB_CurrentMode == USB_MODE_DEVICE)
	{
		if (USB_Options & USB_DEVICE_OPT_LOWSPEED)
		  USB_Device_SetLowSpeed();
		else
		  USB_Device_SetHighSpeed();
		  
		USB_INT_Enable(USB_INT_VBUS);
	}
	#endif

	#if defined(USB_CAN_BE_HOST)
	USB_INT_Enable(USB_INT_VBERRI);
	
	if (USB_CurrentMode == USB_MODE_HOST)
	  USB_Host_PrepareForDeviceConnect();
	#endif
	
	#if defined(USB_CAN_BE_DEVICE)
	USB_Descriptor_Device_t* DeviceDescriptorPtr;
	uint16_t                 DeviceDescriptorSize;

	if (USB_GetDescriptor((DTYPE_Device << 8), 0, (void*)&DeviceDescriptorPtr, &DeviceDescriptorSize) == true)
	{		  
		#if defined(USE_RAM_DESCRIPTORS)
			USB_ControlEndpointSize = DeviceDescriptorPtr->Endpoint0Size;
		#elif defined(USE_EEPROM_DESCRIPTORS)
			USB_ControlEndpointSize = eeprom_read_byte(&DeviceDescriptorPtr->Endpoint0Size);
		#else
			USB_ControlEndpointSize = pgm_read_byte(&DeviceDescriptorPtr->Endpoint0Size);
		#endif
	}
	#endif

	USB_Attach();
	
	#if defined(USB_DEVICE_ONLY)	
	USB_INT_Enable(USB_INT_SUSPEND);
	USB_INT_Enable(USB_INT_EORSTI);	
	#elif defined(USB_HOST_ONLY)
	USB_Host_HostMode_On();
	#else
	if (USB_CurrentMode == USB_MODE_DEVICE)
	{
		USB_INT_Enable(USB_INT_SUSPEND);
		USB_INT_Enable(USB_INT_EORSTI);
	}
	else if (USB_CurrentMode == USB_MODE_HOST)
	{
		USB_Host_HostMode_On();
	}
	#endif
	
	#if defined(USB_CAN_BE_BOTH)
	USB_InitTaskPointer();
	#endif
}
