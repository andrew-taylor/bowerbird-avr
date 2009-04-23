//
// Description: Common defines, macros and inline functions for the system//
//
// Author: Cameron Stone <camerons@cse.unsw.edu.au>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution

#ifndef __COMMON_H__

// the default sampling frequency for all the microphones
// FIXME add preprocessor check to ensure frequency will work
#define DEFAULT_AUDIO_SAMPLE_FREQUENCY      42000
#define DEFAULT_AUDIO_SAMPLE_FREQUENCY_KHZ	(DEFAULT_AUDIO_SAMPLE_FREQUENCY / 1000)

// 8 audio streams
#define AUDIO_CHANNELS 1

// convert to kHz so it can fit into 16 bits
#define F_CPU_KHZ (F_CPU / 1000)

#define TRUE          1
#define FALSE         0



#endif // __COMMON_H__