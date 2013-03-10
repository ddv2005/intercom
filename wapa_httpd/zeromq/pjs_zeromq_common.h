/*
 * pjs_zeromq_common.h
 *
 *  Created on: Feb 25, 2013
 *      Author: ddv
 */

#ifndef PJS_ZEROMQ_COMMON_H_
#define PJS_ZEROMQ_COMMON_H_

#pragma pack(push)
#pragma pack(1)

#define PJSAMZ_TYPE_SIZE	1

#define PJSAMZ_TYPE_WAVE	'W'

typedef struct
{
	int	clock_rate;
	unsigned char		channels;
}pjs_audio_monitor_zeromq_packet_t;


#pragma pack(pop)


#endif /* PJS_ZEROMQ_COMMON_H_ */
