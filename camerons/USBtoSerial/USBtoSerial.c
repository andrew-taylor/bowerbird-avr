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
#include <stdarg.h>
#include <string.h>
#include <avr/wdt.h>

// defining this prevents the watchdog from actually resetting the beagle
#define WATCHDOG_DRY_RUN

#define MAX_LINE_LENGTH 1024
#define COMMAND_PREFIX "#=# AVR "

#define WATCHDOG_CMD "watchdog"
#define WATCHDOG_ENABLE "enable"
#define WATCHDOG_DISABLE "disable"
#define WATCHDOG_TIMEOUT_S 600
#define WATCHDOG_PRESCALER 1024

#define LCD_CMD "lcd"
#define LCD_CLEAR_CMD "clear"
#define POWER_LCD "lcd"
#define POWER_PIN_LCD 7
#define DEVICE_NAME_LCD "LCD Panel"
// These lines need to be less than 24 characters
#define LCD_STARTUP_LINE1 "Bowerbird Starting..."
#define LCD_STARTUP_LINE2 ""

#define NEW_CABLE

#define POWER_PORT PORTC
#define BEAGLE_RESET_CMD "REALLY reset the Beagleboard"
#ifdef NEW_CABLE
#define POWER_PIN_BEAGLE 0
#else
#define POWER_PIN_BEAGLE 7
#endif
#define BEAGLE_RESET_DURATION_IN_S 10
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

/** Buffer to store the last AVR command received on the serial port */
char CommandBuffer[MAX_LINE_LENGTH];
short CommandBufferIndex = 0;
short CommandPrefixIndex = 0;
short InCommand = 0;

// buffer to store the second line so it can be "scrolled" up to the first line
// on the next write
short LCD_on = 1;
char LCD_Buffer[LCD_LINE_LENGTH + 1];
short LCD_Lines = 0;

// counter to store how many seconds since we last heard from the beagle
int Beagle_Watchdog_Counter;
// counter for delayed beagle restarting
int Beagle_Restart_Countdown;
// flag to store if last reset was due to the internal watchdog
int AVR_watchdog_reset = 0;


/** Main program entry point. This routine configures the hardware required by the application, then
 *  starts the scheduler to run the application tasks.
 */
int main(void)
{
	/* Ring buffer Initialization */
	Buffer_Initialize(&Rx_Buffer);
	Buffer_Initialize(&Tx_Buffer);

	SetupHardware();

	for (;;)
	{
		CDC_Task();
		USB_USBTask();

		// reset the internal watchdog
		wdt_reset();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
	InitialiseAVRWatchdog();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

	// enable pull-ups on all unused ports to reduce power consumption
	PORTB = 0xFF;
	PORTE = 0xFF;
	PORTF = 0xFF;

	// Enable pull-ups on unused pins of PORTD
	// (Pins 3 & 4 are used by serial port (counting from 1))
	PORTD = 0xF3;

	// Enable output on port a and c
	DDRA = 0xFF;
	DDRC = 0xFF;

	// Turn on power to devices that should have it (Beagle at least)
	LCD_PORT |= (1 << POWER_PIN_LCD);
	// Turn everything off
	// (power port pins are active low)
	POWER_PORT = 0xFF;
	PowerOn(POWER_PIN_BEAGLE, 1);
	PowerOn(POWER_PIN_USBHUB, 1);

	/* Hardware Initialization */
	Serial_Init(LineEncoding.BaudRateBPS, true);
	USB_Init();

	// Enable interrupts on USART
	UCSR1B |= (1 << RXCIE1);

	// Initialise the LCD
	lcd_init(LCD_DISP_ON);
	if (strlen(LCD_STARTUP_LINE1))
		WriteStringToLCD(LCD_STARTUP_LINE1);
	if (strlen(LCD_STARTUP_LINE2))
		WriteStringToLCD(LCD_STARTUP_LINE2);

	// if we were reset by the AVR watchdog, then report this:
	if (AVR_watchdog_reset)
		WriteStringToLCD("AVR Internal WDT Reset");
	
	// set up beagle watchdog and restart timer
	InitialiseTimers();
	StartBeagleWatchdog();
}


/** Event handler for the USB_Connect event. This starts the library USB task
 *  to begin the enumeration and USB management process.
 */
void EVENT_USB_Device_Connect(void)
{
	/* Indicate USB enumerating */
	WriteStringToLCD("USB Connected");
}

/** Event handler for the USB_Disconnect event. 
 */
void EVENT_USB_Device_Disconnect(void)
{	
	/* Reset Tx and Rx buffers, device disconnected */
	Buffer_Initialize(&Rx_Buffer);
	Buffer_Initialize(&Tx_Buffer);

	/* Indicate USB not ready */
	WriteStringToLCD("USB Disconnected");
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when
 *  the host set the current configuration of the USB device after enumeration 
 *  - the device endpoints are configured and the CDC management task started.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	/* Indicate USB connected and ready */
	WriteStringToLCD("USB Config Changed");

	/* Setup CDC Notification, Rx and Tx Endpoints */
	if (!(Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPNUM, EP_TYPE_INTERRUPT,
			 ENDPOINT_DIR_IN, CDC_NOTIFICATION_EPSIZE, ENDPOINT_BANK_SINGLE))) {
		WriteStringToLCD("USBConfErr Notify EP");
	}
	
	if (!(Endpoint_ConfigureEndpoint(CDC_TX_EPNUM, EP_TYPE_BULK,
			ENDPOINT_DIR_IN, CDC_TXRX_EPSIZE, ENDPOINT_BANK_SINGLE))) {
		WriteStringToLCD("USBConfErr Transmit EP");
	}							   

	if (!(Endpoint_ConfigureEndpoint(CDC_RX_EPNUM, EP_TYPE_BULK,
			ENDPOINT_DIR_OUT, CDC_TXRX_EPSIZE, ENDPOINT_BANK_SINGLE))) {
		WriteStringToLCD("USBConfErr Receive EP");
	}
}

/** Event handler for the USB_UnhandledControlRequest event. This is used to
 *  catch standard and class specific control requests that are not handled
 *  internally by the USB library (including the CDC control commands,
 *  which are all issued via the control endpoint), so that they can be handled
 *  appropriately for the application.
 */
void EVENT_USB_Device_UnhandledControlRequest(void)
{
	/* Process CDC specific control requests */
	switch (USB_ControlRequest.bRequest)
	{
		case REQ_GetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST 
					| REQTYPE_CLASS | REQREC_INTERFACE)) {	
				/* Acknowledge the SETUP packet, ready for data transfer */
				Endpoint_ClearSETUP();

				/* Write the line coding data to the control endpoint */
				Endpoint_Write_Control_Stream_LE(&LineEncoding, sizeof(LineEncoding));
				
				/* Finalize the stream transfer to send the last packet or clear the host abort */
				Endpoint_ClearOUT();
			}
			
			break;
		case REQ_SetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE
					| REQTYPE_CLASS | REQREC_INTERFACE)) {
				/* Acknowledge the SETUP packet, ready for data transfer */
				Endpoint_ClearSETUP();

				/* Read the line coding data in from the host into the global struct */
				// camerons 23/10/09 - disable this functionality because we're
				// always connecting to a 115200n8 serial device
				//Endpoint_Read_Control_Stream_LE(&LineEncoding, sizeof(LineEncoding));

				/* Finalize the stream transfer to clear the last packet from the host */
				Endpoint_ClearIN();
			}
	
			break;
		case REQ_SetControlLineState:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE 
					| REQTYPE_CLASS | REQREC_INTERFACE)) {				
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


/** Setup the watchdog timer */
void InitialiseTimers()
{
	// save the status register
	unsigned char ucSREG = SREG;
	// disable interrupts to prevent race conditions with 16bit registers
	cli();

	// Maximum timer tick is around 4 seconds, so we'll set the timer
	// to go off once a second, and use a counter.
	// 1 = prescaler * (1 + OCR1A) / F_CPU
	// So OCR1A = (F_CPU / prescaler) - 1
	// NOTE: ensure this value fits into 16bits - change prescaler if necessary
	OCR1A = (uint16_t)(F_CPU / WATCHDOG_PRESCALER - 1);
	OCR3A = (uint16_t)(F_CPU / WATCHDOG_PRESCALER - 1);

	// restore status register (will re-enable interrupts if the were enabled)
	SREG = ucSREG;
}


/** Start the watchdog timer */
void StartBeagleWatchdog()
{
	Beagle_Watchdog_Counter = 0;
	WriteStringToLCD("Beagle Watchdog enabled");
	
	TCCR1B  = (1 << WGM12)  // Clear-timer-on-compare-match-OCR1A (CTC) mode
			| (1 << CS12) | (1 << CS10);  // prescaler divides by 1024
	TIMSK1 |= (1 << OCIE1A); // Enable timer interrupt
}


/** Start the watchdog timer */
void StopBeagleWatchdog()
{
	WriteStringToLCD("Beagle Watchdog disabled");
	TIMSK1 &= ~(1 << OCIE1A); // Disable timer interrupt
	TCCR1B &= ~(1 << CS10) & ~(1 << CS11) & ~(1 << CS12); // disable clock
}


/** Process bytes received on the serial port to see if there's any commands to
 *  the AVR itself. We're looking for strings that start with the #defined
 *  magic string and end with a newline.
 */
void ProcessByte(uint8_t ReceivedByte)
{
	// reset the watchdog
	Beagle_Watchdog_Counter = 0;

	// If we're currently read in a command, then pay attention
	if (InCommand) {
		// check if this is the end of the line
		if (ReceivedByte == '\n' || ReceivedByte == '\r') {
			// null-terminate the string in the buffer
			CommandBuffer[CommandBufferIndex] = '\0';

			// see if we understand the command
			if (strncmp(CommandBuffer, BEAGLE_RESET_CMD, strlen(BEAGLE_RESET_CMD)) == 0) {
				ProcessBeagleResetCommand(CommandBuffer + strlen(BEAGLE_RESET_CMD) + 1);
			}
			else if (strncmp(CommandBuffer, POWER_CMD, strlen(POWER_CMD)) == 0) {
				ProcessPowerCommand(CommandBuffer + strlen(POWER_CMD) + 1);
			}
			else if (strncmp(CommandBuffer, LCD_CMD, strlen(LCD_CMD)) == 0) {
				ProcessLCDCommand(CommandBuffer + strlen(LCD_CMD) + 1);
			}
			else if (strncmp(CommandBuffer, WATCHDOG_CMD, strlen(WATCHDOG_CMD)) == 0) {
				ProcessWatchdogCommand(CommandBuffer + strlen(WATCHDOG_CMD) + 1);
			}
			else if (strncmp(CommandBuffer, RESET_CMD, strlen(RESET_CMD)) == 0) {
				soft_reset();
			}
			else {
				WriteStringToUSB("\r\nGot unknown AVR command '%s'\r\n", CommandBuffer);
				WriteStringToLCD("Unknown command:");
				WriteStringToLCD(CommandBuffer);
			}

			// clear the buffer
			CommandBufferIndex = 0;
			// clear the flag
			InCommand = 0;
		}
		else if (CommandBufferIndex < MAX_LINE_LENGTH) {
			CommandBuffer[CommandBufferIndex++] = ReceivedByte;
		}
	}
	// see if this is a line for the AVR
	else if (ReceivedByte == COMMAND_PREFIX[CommandPrefixIndex]) {
		CommandPrefixIndex++;
		if (CommandPrefixIndex >= strlen(COMMAND_PREFIX)) {
			CommandPrefixIndex = 0;
			InCommand = 1;
		}
	}
	else {
		CommandPrefixIndex = 0;
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
	PowerOn(POWER_PIN_BEAGLE, 0);

	SetDelayedBeagleWakeup(BEAGLE_RESET_DURATION_IN_S);
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
	if (cmd[0] >= '1' && cmd[0] <= '9') {
		is_on_power_port = 1;
		pin = cmd[0] - '1';
#ifdef NEW_CABLE
		pin = 7 - pin;
#endif
		device_name = "pin";
	}
	else if (strncmp(cmd, POWER_MIC, strlen(POWER_MIC)) == 0) {
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
		// remember if the LCD is on so we can ignore writes to it when its off
		LCD_on = new_power_state;
		// handle LCD power commands specially, because it's a GPIO directly to
		// the LCD power pin
		if (new_power_state) {
			// turn on the power to the LCD
			LCD_PORT = (1 << POWER_PIN_LCD);
			// re-initialise the lcd
			lcd_init(LCD_DISP_ON);
		}
		else {
			// shut down all pins to the LCD (including the power enable pin)
			// to stop the LCD panel sourcing power from the control & data lines
			LCD_PORT = 0;
		}
		device_name = DEVICE_NAME_LCD;
		pin = 0;
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
		PowerOn(pin, new_power_state);
	}

	// report to host
	WriteStringToUSB("\r\nAVR Power System: Turned %s %s[%d]\r\n",
				new_power_state ? "on" : "off", device_name, pin);
	WriteStringToLCD("%s: %s[%d]", new_power_state ? "ON" : "OFF", device_name, pin);
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


/** Turn the beagle watchdog on or off */
void ProcessWatchdogCommand(char *cmd)
{
	// parse if it's an enable or disable command
	if (strncmp(cmd, WATCHDOG_ENABLE, strlen(WATCHDOG_ENABLE)) == 0) {
		StartBeagleWatchdog();
	}
	else if (strncmp(cmd, WATCHDOG_DISABLE, strlen(WATCHDOG_DISABLE)) == 0) {
		StopBeagleWatchdog();
	}
	else {
		WriteStringToUSB("\r\nGot unrecognised WATCHDOG command '%s'\r\n", cmd);
		WriteStringToLCD("Unknown watchdog command:");
		WriteStringToLCD(cmd);
	}
}


/** Turn on or off power to the given pin.
 * Abstracted to prevent mistakes with active low connection.
 */
void PowerOn(int pin, int on)
{
	if (on)
		POWER_PORT &= ~(1 << pin);
	else
		POWER_PORT |= (1 << pin);
}


/** Set timer3 to go off after the given number of seconds and wake the
 * beagleboard up again.
 */
void SetDelayedBeagleWakeup(int seconds)
{
	// set the countdown
	Beagle_Restart_Countdown = seconds;

	// start the timer
	TCCR3B  = (1 << WGM32)  // Clear-timer-on-compare-match-OCR3A (CTC) mode
			| (1 << CS32) | (1 << CS30);  // prescaler divides by 1024
	TIMSK3 |= (1 << OCIE3A); // Enable timer interrupt
}


/** Write a single line to the LCD screen. Don't put any newlines in the string
 * because it'll confuse me. (or change this code to support it)
 */
void WriteStringToLCD(char *format, ...)
{
	// prevent lengthy timeout when LCD is off
	if (!LCD_on)
		return;

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


/** ISR to handle when the beagle watchdog timer goes off. This means that we
 * haven't heard from the beagle in too long, so we're going to reset it.
 */
ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
	if (++Beagle_Watchdog_Counter > WATCHDOG_TIMEOUT_S) {
#ifdef WATCHDOG_DRY_RUN
		ProcessLCDCommand("Beagle Watchdog went off");
#else
		ProcessBeagleResetCommand("Beagle Watchdog timer went off");
#endif
		Beagle_Watchdog_Counter = 0;

		// TODO add this to the count in EEPROM
	}
}


/** ISR to handle when the delayed beagle restart timer goes off. This basically
 * requires us to decrement the counter, and when it gets to zero to turn the
 * beagle on, and disable the timer
 */
ISR(TIMER3_COMPA_vect, ISR_BLOCK)
{
	if (--Beagle_Restart_Countdown <= 0) {
		// disable the timer
		TIMSK3 &= ~(1 << OCIE3A); // Disable timer interrupt
		TCCR3B &= ~(1 << CS30) & ~(1 << CS31) & ~(1 << CS32); // disable clock

		// turn power to beagle back on
		PowerOn(POWER_PIN_BEAGLE, 1);
		WriteStringToLCD("ON: Beagleboard");
		WriteStringToUSB("\r\nBeagleboard being turned on again.\r\n");
	}
}


/** ISR to handle the USART receive complete interrupt, fired each time the USART has received a character. This stores the received
 *  character into the Tx_Buffer circular buffer for later transmission to the host.
 */
ISR(USART1_RX_vect, ISR_BLOCK)
{
	uint8_t ReceivedByte = UDR1;

	/* Only store received characters if the USB interface is connected */
	if ((USB_DeviceState == DEVICE_STATE_Configured) && LineEncoding.BaudRateBPS) {
		Buffer_StoreElement(&Tx_Buffer, ReceivedByte);
	}

	// process any special commands to the AVR
	ProcessByte(ReceivedByte);
}


/** Implementation for watchdog initialisation */
void InitialiseAVRWatchdog(void)
{
	// if we had an internal watchdog reset, then clear the flag and add it to
	// our count in non-volatile memory
	if (MCUSR & (1 << WDRF)) {
		MCUSR &= ~(1 << WDRF);
		AVR_watchdog_reset = 1;
		// TODO add this to the count in EEPROM
	}

	// Enable watchdog at maximum timeout (8 seconds)
	// NOTE: if setting this to a small value, make sure that the code will get
	// to the wdt_reset call in the main loop in time.
	wdt_enable(WDTO_8S);

    return;
}

