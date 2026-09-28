#pragma once
#include <stdint.h>
#include <stddef.h>
// Audio owner writes; reader may access only after the freeze release/acquire.
template<size_t N> struct PopRing {
    int16_t samples[N]{};
    size_t next=0, count=0;
    uint64_t total=0;
    __attribute__((always_inline)) inline void append(const int16_t *stereo, size_t frames) {
        for(size_t i=0;i<frames;++i){samples[next]=stereo[2*i];if(++next==N)next=0;}
        count=count+frames>N?N:count+frames;total+=frames;
    }
    uint8_t byte_at(size_t offset) const {
        size_t i=(next+N-count+offset/2)%N;
        return (uint16_t(samples[i]) >> ((offset&1)*8)) & 255;
    }
    uint32_t crc() const {
        uint32_t c=0xffffffff;
        for(size_t i=0;i<count*2;++i){c^=byte_at(i);for(int b=0;b<8;++b)c=(c>>1)^(0xedb88320u & (0u-(c&1)));}
        return ~c;
    }
};
