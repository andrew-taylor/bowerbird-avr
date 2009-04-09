/*
			    RING BUFFER LIBRARY
			    By Dean Camera, 2007
*/

/* heavily specialised by peteg42 at gmail dot com */

#ifndef RINGBUFF_H
#define RINGBUFF_H

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <limits.h>

// Configuration:

// FIXME adjust
/* Buffer length - select static size of created ringbuffers: */
#define BUFF_STATICSIZE 256  // Set to the static ringbuffer size for all ringbuffers (place size after define)

/* Volatile mode - uncomment to make buffers volatile, for use in ISRs, etc: */
#define BUFF_VOLATILE            // Uncomment to cause all ring buffers to become volatile (and atomic if multi-byte) in access

// FIXME adjust
// on overflow we'd want to drop all the samples for the old frame (collection of samples).

/* Drop mode - select behaviour when Buffer_StoreElement called on a full buffer: */
#define BUFF_DROPOLD             // Uncomment to cause full ring buffers to drop the oldest character to make space when full
// #define BUFF_DROPNEW          // Uncomment to cause full ring buffers to drop the new character when full
// #define BUFF_NODROPCHECK      // Uncomment to ignore full ring buffer checks - checking left to user!

// FIXME
/* Underflow behaviour - select behaviour when Buffer_GetElement is called with an empty ringbuffer: */
#define BUFF_EMPTYRETURNSZERO   // Uncomment to return 0 when an empty ringbuffer is read
//#define BUFF_NOEMPTYCHECK      // Uncomment to disable checking of empty ringbuffers - checking left to user!

/* Buffer storage type - set the datatype for the stored data */
typedef int16_t RingBuff_Data_t; // Change to the data type that is going to be stored into the buffer

#if defined(BUFF_STATICSIZE)
	#define BUFF_LENGTH BUFF_STATICSIZE
#else
	#error No buffer length specified!
#endif

#if !(defined(BUFF_DROPOLD) || defined(BUFF_DROPNEW) || defined(BUFF_NODROPCHECK))
	#error No buffer drop mode specified.
#endif

#if defined(BUFF_VOLATILE)
	#define BUFF_MODE volatile
	#define BUFF_ATOMIC_BLOCK ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
#else
	#define BUFF_MODE
	#define BUFF_ATOMIC_BLOCK
#endif

#if (BUFF_STATICSIZE   > LONG_MAX)
	#define RingBuff_Elements_t uint64_t
#elif (BUFF_STATICSIZE > INT_MAX)
	#define RingBuff_Elements_t uint32_t
#elif (BUFF_STATICSIZE > CHAR_MAX)
	#define RingBuff_Elements_t uint16_t
#else
	#define RingBuff_Elements_t uint8_t
#endif

extern RingBuff_Elements_t  RB_Elements;

void            RB_Init(void);
void            RB_Put(RingBuff_Data_t Data);
RingBuff_Data_t RB_Get(void);

#endif
