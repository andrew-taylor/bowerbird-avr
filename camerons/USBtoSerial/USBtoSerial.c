/*
             LUFA Library
     Copyright (C) Dean Camera, 2009.
              
  dean [at] fourwalledcubicle [dot] com
      www.fourwalledcubicle.com
*/

/*
  Copyright 2009  Dean Camera (dean [at] fourwalledcubicle [dot] com)

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

#include "USBtoSerial.h"
#include <util/delay.h>
#include <stdarg.h>
#include <string.h>
#include <avr/wdt.h>


#define MAX_LINE_LENGTH 1024
#define AVR_LINE_MARKER "#!# AVR"

#define LCD_CMD "lcd"
#define LCD_CLEAR_CMD "clear"
#define POWER_LCD "lcd"
#define POWER_PIN_LCD 7
#define DEVICE_NAME_LCD "LCD Panel"
// These lines need to be less than 24 characters
#define LCD_STARTUP_LINE1 "Bowerbird Starting..."
#define LCD_STARTUP_LINE2 "Taylored Industries"
#define LCD_RESTORE_POWER_LINE "LCD Panel Active"

#define POWER_PORT PORTC
#define BEAGLE_RESET_CMD "REALLY reset the Beagleboard"
#define POWER_PIN_BEAGLE 0
#define BEAGLE_RESET_DURATION_IN_US 3000000
#define POWER_CMD "power"
#define POWER_ON "on"
#define POWER_OFF "off"
#define POWER_MIC "usbmic"
#define POWER_PIN_MIC 1
#define DEVICE_NAME_MIC "USB Microphone Array"
#define POWER_USBHUB "usbhub"
#define POWER_PIN_USBHUB 2
#define DEVICE_NAME_USBHUB "USB Hub"
#define POWER_USBHD "usbhd"
#define POWER_PIN_USBHD 3
#define DEVICE_NAME_USBHD "USB Hard Disk"
#define POWER_NEXTG "nextg"
#define POWER_PIN_NEXTG 4
#define DEVICE_NAME_NEXTG "NextG Modem"
#define POWER_WIFI "wifi"
#define POWER_PIN_WIFI 5
#define DEVICE_NAME_WIFI "Wireless"

#define RESET_CMD "REALLY reset the AVR"


#define soft_reset()        \
do {                        \
    wdt_enable(WDTO_15MS);  \
    for(;;) {}              \
} while(0)

// Prototype for watchdog initialisation (to disable it on boot)
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));


/* Globals: */
/** Contains the current baud rate and other settings of the virtual serial port.
 *
 *  These values are set by the host via a class-specific request, and the physical USART should be reconfigured to match the
 *  new settings each time they are changed by the host.
 */
CDC_Line_Coding_t LineEncoding = { 
	.BaudRateBPS = 115200,
	.CharFormat  = OneStopBit,
	.ParityType  = Parity_None,
	.DataBits    = 8
};

/** Ring (circular) buffer to hold the RX data - data from the host to the attached device on the serial port. */
RingBuff_t Rx_Buffer;

/** Ring (circular) buffer to hold the TX data - data from the attached device on the serial port to the host. */
RingBuff_t Tx_Buffer;

/** Flag to indicate if the USART is currently transmitting data from the Rx_Buffer circular buffer. */
volatile bool Transmitting = false;

/** Buffer to store the last line received on the serial port so we can process
 * commands to the AVR
 */
char SerialBuffer[MAX_LINE_LENGTH];
short SerialBufferIndex = 0;

// buffer to store the second line so it can be "scrolled" up to the first line
// on the next write
char LCD_Buffer[LCD_LINE_LENGTH + 1];
short LCD_Lines = 0;


/** Main program entry point. This routine configures the hardware required by the application, then
 *  starts the scheduler to run the application tasks.
 */
int main(void)
{
	/* Ring buffer Initialization */
	Buffer_Initialize(&Rx_Buffer);
	Buffer_Initialize(&Tx_Buffer);

	SetupHardware();

//	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
	
	for (;;)
	{
		CDC_Task();
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

	// Enable output on port a and c
	DDRA = 0xFF;
	DDRC = 0xFF;

	// Turn on power to devices that should have it (Beagle at least)
	LCD_PORT |= (1 << POWER_PIN_LCD);
	// (power port pins are active low)
	POWER_PORT = 0xFF & ~(1 << POWER_PIN_BEAGLE) & ~(1 << POWER_PIN_USBHUB);

	/* Hardware Initialization */
	Serial_Init(LineEncoding.BaudRateBPS, true);
//	LEDs_Init();
	USB_Init();

	// Enable interrupts on USART
	UCSR1B |= (1 << RXCIE1);

	// Initialise the LCD
	lcd_init(LCD_DISP_ON);
	if (strlen(LCD_STARTUP_LINE1))
		WriteStringToLCD(LCD_STARTUP_LINE1);
	if (strlen(LCD_STARTUP_LINE2))
		WriteStringToLCD(LCD_STARTUP_LINE2);
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs and
 *  starts the library USB task to begin the enumeration and USB management process.
 */
void EVENT_USB_Device_Connect(void)
{
	/* Indicate USB enumerating */
//	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs and stops the USB management and CDC management tasks.
 */
void EVENT_USB_Device_Disconnect(void)
{	
	/* Reset Tx and Rx buffers, device disconnected */
	Buffer_Initialize(&Rx_Buffer);
	Buffer_Initialize(&Tx_Buffer);

	/* Indicate USB not ready */
//	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host set the current configuration
 *  of the USB device after enumeration - the device endpoints are configured and the CDC management task started.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	/* Indicate USB connected and ready */
//	LEDs_SetAllLEDs(LEDMASK_USB_READY);

	/* Setup CDC Notification, Rx and Tx Endpoints */
	if (!(Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPNUM, EP_TYPE_INTERRUPT,
		                             ENDPOINT_DIR_IN, CDC_NOTIFICATION_EPSIZE,
	                                 ENDPOINT_BANK_SINGLE)))
	{
//		LEDs_SetAllLEDs(LEDMASK_USB_ERROR);
	}
	
	if (!(Endpoint_ConfigureEndpoint(CDC_TX_EPNUM, EP_TYPE_BULK,
		                             ENDPOINT_DIR_IN, CDC_TXRX_EPSIZE,
	                                 ENDPOINT_BANK_SINGLE)))
	{
//		LEDs_SetAllLEDs(LEDMASK_USB_ERROR);
	}							   

	if (!(Endpoint_ConfigureEndpoint(CDC_RX_EPNUM, EP_TYPE_BULK,
		                             ENDPOINT_DIR_OUT, CDC_TXRX_EPSIZE,
	                                 ENDPOINT_BANK_SINGLE)))
	{
//		LEDs_SetAllLEDs(LEDMASK_USB_ERROR);
	}

	/* Reset line encoding baud rate so that the host knows to send new values */
	//LineEncoding.BaudRateBPS = 0;
}

/** Event handler for the USB_UnhandledControlRequest event. This is used to catch standard and class specific
 *  control requests that are not handled internally by the USB library (including the CDC control commands,
 *  which are all issued via the control endpoint), so that they can be handled appropriately for the application.
 */
void EVENT_USB_Device_UnhandledControlRequest(void)
{
	/* Process CDC specific control requests */
	switch (USB_ControlRequest.bRequest)
	{
		case REQ_GetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{	
				/* Acknowledge the SETUP packet, ready for data transfer */
				Endpoint_ClearSETUP();

				/* Write the line coding data to the control endpoint */
				Endpoint_Write_Control_Stream_LE(&LineEncoding, sizeof(LineEncoding));
				
				/* Finalize the stream transfer to send the last packet or clear the host abort */
				Endpoint_ClearOUT();
			}
			
			break;
		case REQ_SetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				/* Acknowledge the SETUP packet, ready for data transfer */
				Endpoint_ClearSETUP();

				/* Read the line coding data in from the host into the global struct */
				// camerons 23/10/09 - disable this functionality because it
				// keeps changing the baud rate to low values
				//Endpoint_Read_Control_Stream_LE(&LineEncoding, sizeof(LineEncoding));

				/* Finalize the stream transfer to clear the last packet from the host */
				Endpoint_ClearIN();
				
				/* Reconfigure the USART with the new settings */
				// camerons 23/10/09 - disable this functionality because it
				// keeps changing the baud rate to low values
				//ReconfigureUSART();
			}
	
			break;
		case REQ_SetControlLineState:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{				
				/* Acknowledge the SETUP packet, ready for data transfer */
				Endpoint_ClearSETUP();
				
				/* NOTE: Here you can read in the line state mask from the host, to get the current state of the output handshake
				         lines. The mask is read in from the wValue parameter in USB_ControlRequest, and can be masked against the
						 CONTROL_LINE_OUT_* masks to determine the RTS and DTR line states using the following code:
				*/

				Endpoint_ClearStatusStage();
			}
	
			break;
	}
}

/** Task to manage CDC data transmission and reception to and from the host, from and to the physical USART. */
void CDC_Task(void)
{
	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
		return;
	  
#if 0
	/* NOTE: Here you can use the notification endpoint to send back line state changes to the host, for the special RS-232
			 handshake signal lines (and some error states), via the CONTROL_LINE_IN_* masks and the following code:
	*/

	USB_Notification_Header_t Notification = (USB_Notification_Header_t)
		{
			.NotificationType = (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE),
			.Notification     = NOTIF_SerialState,
			.wValue           = 0,
			.wIndex           = 0,
			.wLength          = sizeof(uint16_t),
		};
		
	uint16_t LineStateMask;
	
	// Set LineStateMask here to a mask of CONTROL_LINE_IN_* masks to set the input handshake line states to send to the host
	
	Endpoint_SelectEndpoint(CDC_NOTIFICATION_EPNUM);
	Endpoint_Write_Stream_LE(&Notification, sizeof(Notification));
	Endpoint_Write_Stream_LE(&LineStateMask, sizeof(LineStateMask));
	Endpoint_ClearIN();
#endif

	/* Select the Serial Rx Endpoint */
	Endpoint_SelectEndpoint(CDC_RX_EPNUM);
	
	/* Check to see if a packet has been received from the host */
	if (Endpoint_IsOUTReceived())
	{
		/* Read the bytes in from the endpoint into the buffer while space is available */
		while (Endpoint_BytesInEndpoint() && (Rx_Buffer.Elements != BUFF_STATICSIZE))
		{
			/* Store each character from the endpoint */
			Buffer_StoreElement(&Rx_Buffer, Endpoint_Read_Byte());
		}
		
		/* Check to see if all bytes in the current packet have been read */
		if (!(Endpoint_BytesInEndpoint()))
		{
			/* Clear the endpoint buffer */
			Endpoint_ClearOUT();
		}
	}
	
	/* Check if Rx buffer contains data - if so, send it */
	if (Rx_Buffer.Elements)
	  Serial_TxByte(Buffer_GetElement(&Rx_Buffer));

	/* Select the Serial Tx Endpoint */
	Endpoint_SelectEndpoint(CDC_TX_EPNUM);

	/* Check if the Tx buffer contains anything to be sent to the host */
	if ((Tx_Buffer.Elements) && LineEncoding.BaudRateBPS)
	{
		/* Wait until Serial Tx Endpoint Ready for Read/Write */
		Endpoint_WaitUntilReady();
		
		/* Write the bytes from the buffer to the endpoint while space is available */
		while (Tx_Buffer.Elements && Endpoint_IsReadWriteAllowed())
		{
			uint8_t DataByte = Buffer_GetElement(&Tx_Buffer);
			/* Write each byte retreived from the buffer to the endpoint */
			Endpoint_Write_Byte(DataByte);
		}
		
		/* Remember if the packet to send completely fills the endpoint */
		bool IsFull = (Endpoint_BytesInEndpoint() == CDC_TXRX_EPSIZE);
		
		/* Send the data */
		Endpoint_ClearIN();

		/* If no more data to send and the last packet filled the endpoint, send an empty packet to release
		 * the buffer on the receiver (otherwise all data will be cached until a non-full packet is received) */
		if (IsFull && !(Tx_Buffer.Elements))
		{
			/* Wait until Serial Tx Endpoint Ready for Read/Write */
			Endpoint_WaitUntilReady();
				
			/* Send an empty packet to terminate the transfer */
			Endpoint_ClearIN();
		}
	}
}


/** Process bytes received on the serial port to see if there's any commands to
 *  the AVR itself
 */
void ProcessByte(uint8_t ReceivedByte)
{
	// check if this is the end of the line
	if (ReceivedByte == '\n' || ReceivedByte == '\r') {
		// null-terminate the string in the buffer
		SerialBuffer[SerialBufferIndex] = '\0';
		// see if this is a line for the AVR
		if (strncmp(SerialBuffer, AVR_LINE_MARKER, strlen(AVR_LINE_MARKER)) == 0) {
			char *cmd = SerialBuffer + strlen(AVR_LINE_MARKER) + 1;
			
			// see if we understand the command
			if (strncmp(cmd, BEAGLE_RESET_CMD, strlen(BEAGLE_RESET_CMD)) == 0) {
				ProcessBeagleResetCommand(cmd + strlen(BEAGLE_RESET_CMD) + 1);
			}
			else if (strncmp(cmd, POWER_CMD, strlen(POWER_CMD)) == 0) {
				ProcessPowerCommand(cmd + strlen(POWER_CMD) + 1);
			}
			else if (strncmp(cmd, LCD_CMD, strlen(LCD_CMD)) == 0) {
				ProcessLCDCommand(cmd + strlen(LCD_CMD) + 1);
			}
			else if (strncmp(cmd, RESET_CMD, strlen(RESET_CMD)) == 0) {
				soft_reset();
			}
			else {
				WriteStringToUSB("\r\nGot unknown AVR command '%s'\r\n", cmd);
				WriteStringToLCD("Unknown command:");
				WriteStringToLCD(cmd);
			}
		}

		// clear the buffer
		SerialBufferIndex = 0;
	}
	else if (SerialBufferIndex < MAX_LINE_LENGTH) {
		SerialBuffer[SerialBufferIndex++] = ReceivedByte;
	}
}


/** Handle a command to reset the beagleboard. This involves turning off power
 *  to the beagle power pin for a while, then turning it back on.
 */
void ProcessBeagleResetCommand(char *cmd)
{
	// ignore argument for now
	// report to host that we're resetting the beagle
	WriteStringToUSB("\r\nResetting Beagleboard (%s)\r\n", cmd);
	WriteStringToLCD("OFF: Beagleboard");

	// turn off power to beagle
	POWER_PORT &= ~(1 << POWER_PIN_BEAGLE);

	// sleep for a while (we don't care if this blocks everything else on the
	// AVR because the beagle is the only input we really care about.
	_delay_us(BEAGLE_RESET_DURATION_IN_US);

	// turn power to beagle back on
	POWER_PORT |= (1 << POWER_PIN_BEAGLE);
	WriteStringToLCD("ON: Beagleboard");
}


/** Handle a command to turn on or off power to one of the devices.
 *  This is just toggling gpios.
 */
void ProcessPowerCommand(char *cmd)
{
	int new_power_state, is_on_power_port, pin;
	char *device_name;

	// parse if it's an on or an off command
	if (strncmp(cmd, POWER_ON, strlen(POWER_ON)) == 0) {
		new_power_state = 1;
		cmd += strlen(POWER_ON) + 1;
	}
	else if (strncmp(cmd, POWER_OFF, strlen(POWER_OFF)) == 0) {
		new_power_state = 0;
		cmd += strlen(POWER_OFF) + 1;
	}
	else {
		WriteStringToUSB("\r\nGot unrecognised POWER command '%s'\r\n", cmd);
		WriteStringToLCD("Unknown power command:");
		WriteStringToLCD(cmd);
		return;
	}

	// now parse what device to turn on or off
	if (strncmp(cmd, POWER_MIC, strlen(POWER_MIC)) == 0) {
		is_on_power_port = 1;
		pin = POWER_PIN_MIC;
		device_name = DEVICE_NAME_MIC;
	}
	else if (strncmp(cmd, POWER_NEXTG, strlen(POWER_NEXTG)) == 0) {
		is_on_power_port = 1;
		pin = POWER_PIN_NEXTG;
		device_name = DEVICE_NAME_NEXTG;
	}
	else if (strncmp(cmd, POWER_WIFI, strlen(POWER_WIFI)) == 0) {
		is_on_power_port = 1;
		pin = POWER_PIN_WIFI;
		device_name = DEVICE_NAME_WIFI;
	}
	else if (strncmp(cmd, POWER_USBHUB, strlen(POWER_USBHUB)) == 0) {
		is_on_power_port = 1;
		pin = POWER_PIN_USBHUB;
		device_name = DEVICE_NAME_USBHUB;
	}
	else if (strncmp(cmd, POWER_USBHD, strlen(POWER_USBHD)) == 0) {
		is_on_power_port = 1;
		pin = POWER_PIN_USBHD;
		device_name = DEVICE_NAME_USBHD;
	}
	else if (strncmp(cmd, POWER_LCD, strlen(POWER_LCD)) == 0) {
		is_on_power_port = 0;
		// handle LCD power commands specially, because it's a GPIO directly to
		// the LCD power pin
		if (new_power_state) {
			// turn on the power to the LCD
			LCD_PORT = (1 << POWER_PIN_LCD);
			// re-initialise the lcd
			lcd_init(LCD_DISP_ON);
			// write a message to show we've powered it back on
			WriteStringToLCD(LCD_RESTORE_POWER_LINE);
		}
		else {
			// shut down all pins to the LCD (including the power enable pin)
			// to stop the LCD panel sourcing power from the control & data lines
			LCD_PORT = 0;
		}
		device_name = DEVICE_NAME_LCD;
	}
	else {
		WriteStringToUSB("\r\nGot request to turn %s unknown device: '%s'\r\n", 
				new_power_state ? "on" : "off", cmd);
		WriteStringToLCD("Unknown power device:");
		WriteStringToLCD(cmd);
		return;
	}

	// carry out command (if it's on the power port)
	if (is_on_power_port) {
		// power port pins are active low
		if (new_power_state)
			POWER_PORT &= ~(1 << pin);
		else
			POWER_PORT |= (1 << pin);
	}

	// report to host
	WriteStringToUSB("\r\nAVR Power System: Turned %s %s\r\n",
				new_power_state ? "on" : "off", device_name);
	WriteStringToLCD("%s: %s", new_power_state ? "ON" : "OFF", device_name);
}


/** Handle a command to put a message on the LCD.
 *  String should be up to two lines of 24 or less characters
 */
void ProcessLCDCommand(char *cmd)
{
	if (strncmp(cmd, LCD_CLEAR_CMD, strlen(LCD_CLEAR_CMD)) == 0) {
		lcd_clrscr();
		// clear the buffer
		LCD_Buffer[0] = '\0';
		LCD_Lines = 0;
		WriteStringToUSB("\r\nCleared LCD screen\r\n");
	}
	else {
		WriteStringToLCD(cmd);
		WriteStringToUSB("\r\nSent to LCD: '%s'\r\n", cmd);
	}
}


/** Write a single line to the LCD screen. Don't put any newlines in the string
 * because it'll confuse me. (or change this code to support it)
 */
void WriteStringToLCD(char *format, ...)
{
	char string[MAX_LINE_LENGTH];
	va_list ap;
	va_start(ap, format);
	vsnprintf(string, MAX_LINE_LENGTH, format, ap);
	va_end(ap);

	// handle scrolling of lines
	switch (LCD_Lines) {
		case 2:
			lcd_clrscr();
			lcd_puts(LCD_Buffer);
		case 1:
			lcd_putc('\n');
			strncpy(LCD_Buffer, string, LCD_LINE_LENGTH);
		case 0:
			lcd_puts(string);
			break;
	}

	if (LCD_Lines < 2)
		LCD_Lines++;
}


/** Write to the USB endpoint. This is actually done by pushing the string onto
 * the ring buffer, so we can't get race conditions
 */
void WriteStringToUSB(char *format, ...)
{
	int i, len;

	char string[MAX_LINE_LENGTH];
	va_list ap;
	va_start(ap, format);
	vsnprintf(string, MAX_LINE_LENGTH, format, ap);
	va_end(ap);

	len = strlen(string);
	for (i = 0; i < len; ++i)
		Buffer_StoreElement(&Tx_Buffer, string[i]);
}


/** ISR to handle the USART receive complete interrupt, fired each time the USART has received a character. This stores the received
 *  character into the Tx_Buffer circular buffer for later transmission to the host.
 */
ISR(USART1_RX_vect, ISR_BLOCK)
{
	uint8_t ReceivedByte = UDR1;

	/* Only store received characters if the USB interface is connected */
	if ((USB_DeviceState == DEVICE_STATE_Configured) && LineEncoding.BaudRateBPS)
	{
		Buffer_StoreElement(&Tx_Buffer, ReceivedByte);

		/*/ show the byte on pin A0
		int i, last = 0, this, delay = 1;
		for (i = 0; i < 8; ++i) {
			this = ((ReceivedByte >> i) & 0x01);
			if (this == last)
				PORTA = this - 1;
			_delay_us(delay);
			PORTA = 0 - this;
			last = this;
			_delay_us(6 * delay);
		}
		if (last == 0) {
			PORTA = -1;
			_delay_us(delay);
		}
		PORTA = 0;//*/
	}

	// process any special commands to the AVR
	ProcessByte(ReceivedByte);
}

/** Reconfigures the USART to match the current serial port settings issued by the host as closely as possible. */
void ReconfigureUSART(void)
{
	uint8_t ConfigMask = 0;

	/* Determine parity - non odd/even parity mode defaults to no parity */
	if (LineEncoding.ParityType == Parity_Odd)
	  ConfigMask = ((1 << UPM11) | (1 << UPM10));
	else if (LineEncoding.ParityType == Parity_Even)
	  ConfigMask = (1 << UPM11);

	/* Determine stop bits - 1.5 stop bits is set as 1 stop bit due to hardware limitations */
	if (LineEncoding.CharFormat == TwoStopBits)
	  ConfigMask |= (1 << USBS1);

	/* Determine data size - 5, 6, 7, or 8 bits are supported */
	if (LineEncoding.DataBits == 6)
	  ConfigMask |= (1 << UCSZ10);
	else if (LineEncoding.DataBits == 7)
	  ConfigMask |= (1 << UCSZ11);
	else if (LineEncoding.DataBits == 8)
	  ConfigMask |= ((1 << UCSZ11) | (1 << UCSZ10));
	
	/* Enable double speed, gives better error percentages at 8MHz */
	UCSR1A = (1 << U2X1);
	
	/* Enable transmit and receive modules and interrupts */
	UCSR1B = ((1 << RXCIE1) | (1 << TXEN1) | (1 << RXEN1));

	/* Set the USART mode to the mask generated by the Line Coding options */
	UCSR1C = ConfigMask;
	
	/* Set the USART baud rate register to the desired baud rate value */
	UBRR1  = SERIAL_2X_UBBRVAL((uint16_t)LineEncoding.BaudRateBPS);
}


// Implementation for watchdog initialisation (to disable it on boot)
void wdt_init(void)
{
    MCUSR = 0;
    wdt_disable();

    return;
}

