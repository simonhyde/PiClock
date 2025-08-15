#ifndef __INCLUDED_PICLOCK_TZINFO_H
#define __INCLUDED_PICLOCK_TZINFO_H
#include <chrono>
#include <date/tz.h>
typedef date::local_time<std::chrono::system_clock::duration> time_info;

typedef std::chrono::time_point<std::chrono::system_clock> sys_clock_data;
#endif
