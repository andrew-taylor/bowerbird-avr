/**
 * Description: Common defines, macros and inline functions for the system
 *
 * Author: Cameron Stone <camerons@cse.unsw.edu.au>, (C) 2009
 *
 * Copyright: See COPYING file that comes with this distribution
 */

#ifndef __SHARED_H__
#define __SHARED_H__

#define TRUE          1
#define FALSE         0

/** duplicated from MyUSB/Drivers/USB/LowLevel/Endpoint.h because
 * I need it in the assembly code and that file has C function declarations */
#include <MyUSB/Drivers/USB/LowLevel/USBMode.h>
#if defined(USB_FULL_CONTROLLER) || defined(USB_MODIFIED_FULL_CONTROLLER)
/** Total number of endpoints (including the default control endpoint at
 * address 0) which may be used in the device. Different USB AVR models support
 * different amounts of endpoints, this value reflects the maximum number of
 * endpoints for the currently selected AVR model. */
#define ENDPOINT_MAX_ENDPOINTS                  7
/** Size in bytes of the largest endpoint bank size possible in the device. Not
 * all banks on each AVR model supports the largest bank size possible on the
 * device; different endpoint numbers support different maximum bank sizes.
 * This value reflects the largest possible bank of any endpoint on the
 * currently selected USB AVR model. */
#define ENDPOINT_MAX_SIZE                       256
#else
#define ENDPOINT_MAX_ENDPOINTS                  5
#define ENDPOINT_MAX_SIZE                       64
#endif

#define AUDIO_STREAM_EPNUM          1
#define AUDIO_STREAM_EPSIZE         ENDPOINT_MAX_SIZE
#define ENDPOINT_EPNUM_MASK			0b111


/** Copied from kernel source ./sound/usb/usbaudio.h
 * cs endpoint attributes */
#define EP_CS_ATTR_SAMPLE_RATE		0x01
#define EP_CS_ATTR_PITCH_CONTROL	0x02
#define EP_CS_ATTR_FILL_MAX		0x80
/** Endpoint Control Selectors */
#define SAMPLING_FREQ_CONTROL      0x01
#define PITCH_CONTROL              0x02


/**
 * Global register variables.
 */
#ifdef __ASSEMBLER__
#define usb_ep_save	r17
#define	pair_lsb	r26
#define	pair_msb	r27
#define isr_iter	r28
#define	temp_reg	r29
#endif /* __ASSEMBLER__ */


/** the default sampling frequency for all the microphones
 * FIXME add preprocessor check to ensure frequency will work */
#define LOWEST_AUDIO_SAMPLE_FREQUENCY		2000
#define HIGHEST_AUDIO_SAMPLE_FREQUENCY		80000
#define DEFAULT_AUDIO_SAMPLE_FREQUENCY      16000


/** 8 audio streams */
#define AUDIO_CHANNELS	1


/** sample size in bytes, which is sizeof(uint16_t) = 2, but we can't put the
 * calculation into the code because we need it in assembly, which can't handle
 * the file (stdint.h) that defines uint16_t because of the typedefs in it.
 */
#define SAMPLE_SIZE		2
#define AUDIO_STREAM_FULL_THRESHOLD (AUDIO_STREAM_EPSIZE - ((AUDIO_CHANNELS * SAMPLE_SIZE) - 1))


/** Name the SPI-specific registers/bits
 * see avr-lib's io.h for the basic definitions. */
#define DD_SS       PINB0
#define DD_SCK      PINB1
#define DD_MOSI     PINB2
#define DD_MISO     PINB3
#define DD_OC1A		PINB5

#endif // __SHARED_H__
