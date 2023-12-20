/*
 * uniex_time_utils.cpp
 *
 *  Created on: Nov 25, 2019
 *      Author: Dmitry
 */
#ifdef ESP32
#include <Arduino.h>
#endif
#include "uniex_time_utils.h"
#include <time.h>
#include <sys/time.h>
#include "../uni.h"

int64_t uniex_now_real()
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return (int64_t)tp.tv_sec * (int64_t)1000 + (int64_t)tp.tv_usec / (int64_t)1000;
}
#ifdef ESP32
int64_t uniex_now()
{
	return millis();
}
#else
int64_t uniex_now()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * (int64_t)1000 + (int64_t)ts.tv_nsec / (int64_t)1000000;
}
#endif
int64_t uniex_now_usec()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * (int64_t)1000000 + (int64_t)ts.tv_nsec / (int64_t)1000;
}

int64_t uniex_now_real_usec()
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (int64_t)ts.tv_sec * (int64_t)1000000 + (int64_t)ts.tv_nsec / (int64_t)1000;
}
#ifndef ESP32
int64_t uniex_now_raw()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return (int64_t)ts.tv_sec * (int64_t)1000 + (int64_t)ts.tv_nsec / (int64_t)1000000;
}

int64_t uniex_now_raw_usec()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return (int64_t)ts.tv_sec * (int64_t)1000000 + (int64_t)ts.tv_nsec / (int64_t)1000;
}
#endif

void uniex_measure_delay_c::measure(const char *str)
{
	int64_t now = uniex_now();
	if (m_last)
	{
		if ((now - m_last) >= m_threshold)
		{
			ULOG(1, "%s %d %d", str, now - m_last, m_lastValue);
		}
	}
	m_lastValue = now - m_last;
	m_last = now;
}


