/*
         Advanced Configurable Ring Buffer Library
                 (C) Dean Camera, 2007

          dean [at) fourwalledcubicle <dot} com
               www.fourwalledcubicle.com
*/

/* heavily specialised by peteg42 at gmail dot com */

#include "RingBuffer.h"

RingBuff_Data_t      RB_Buffer[BUFF_LENGTH];
RingBuff_Data_t*     RB_InPtr;
RingBuff_Data_t*     RB_OutPtr;
RingBuff_Elements_t  RB_Elements;

void RB_Init(void)
{
  BUFF_ATOMIC_BLOCK
    {
      RB_InPtr    = RB_Buffer;
      RB_OutPtr   = RB_Buffer;
      RB_Elements = 0;
    }
}

void RB_Put(RingBuff_Data_t Data)
{
  BUFF_ATOMIC_BLOCK
    {

#if defined(BUFF_DROPOLD)

      if (RB_Elements == BUFF_LENGTH) {
	RB_OutPtr++;

	if (RB_OutPtr == &RB_Buffer[BUFF_LENGTH])
	  RB_OutPtr = RB_Buffer;
      } else {
	RB_Elements++;
      }

#elif defined(BUFF_DROPNEW)

      if (RB_Elements == BUFF_LENGTH)
	return;

      RB_Elements++;

#elif defined(BUFF_NODROPCHECK)
      RB_Elements++;
#endif

      *RB_InPtr = Data;
      RB_InPtr++;

      if(RB_InPtr == &RB_Buffer[BUFF_LENGTH])
	RB_InPtr = RB_Buffer;
    }
}

RingBuff_Data_t RB_Get(void)
{
  volatile RingBuff_Data_t BuffData;

  BUFF_ATOMIC_BLOCK
    {
#if defined(BUFF_EMPTYRETURNSZERO)
      if (!(RB_Elements))
	return 0;
#elif !defined(BUFF_NOEMPTYCHECK)
#error No empty buffer check behaviour specified.
#endif

      BuffData = *RB_OutPtr;
      RB_OutPtr++;
      RB_Elements--;

      if (RB_OutPtr == &RB_Buffer[BUFF_LENGTH])
	RB_OutPtr = RB_Buffer;
    }

  return BuffData;
}
