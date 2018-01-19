#ifndef __ZLO_H__
#define __ZLO_H__

#include <stdint.h>

typedef struct zlo_header_t {
    uint8_t ip_version;
    uint8_t nsignals;
    uint8_t word_size;
    uint8_t _res;
    uint32_t burst_size_w; // little endian
} zlo_header_t;


#endif // __ZLO_H__
