#ifndef HB_TIME_H
#define HB_TIME_H


#include <stdint.h>


#define HB_TSTAMP_S 1
#define HB_TSTAMP_MS 1000
#define HB_TSTAMP_US 1000000
#define HB_TSTAMP_NS 1000000000


uint64_t hb_tstamp();
uint64_t hb_tstamp_convert(uint64_t tstamp, uint32_t from, uint32_t to);

#endif