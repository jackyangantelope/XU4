#pragma once
#include "time.h"
inline uint64_t get_absolute_time(){return time_us_64();}
inline uint32_t to_ms_since_boot(uint64_t us){return us/1000;}
void sleep_us(unsigned);
inline void sleep_ms(unsigned ms){sleep_us(ms*1000);}
