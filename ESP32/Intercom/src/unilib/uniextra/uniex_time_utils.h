/*
 * uniex_time_utils.h
 *
 *  Created on: Nov 25, 2019
 *      Author: Dmitry
 */

#ifndef UNIEX_TIME_UTILS_H_
#define UNIEX_TIME_UTILS_H_

#include <stdint.h>

int64_t uniex_now_real();
int64_t uniex_now();
int64_t uniex_now_raw();
int64_t uniex_now_usec();
int64_t uniex_now_raw_usec();
int64_t uniex_now_real_usec();

class uniex_measure_delay_c
{
protected:
	int64_t m_last;
	int64_t	m_threshold;
	int64_t m_lastValue;
public:
	uniex_measure_delay_c(int64_t	threshold):m_last(0),m_threshold(threshold),m_lastValue(0)
	{
	}

	void reset()
	{
		m_last = 0;
		m_lastValue = 0;
	}

	void measure(const char * str);
};

#endif /* UNIEX_TIME_UTILS_H_ */
