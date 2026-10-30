#pragma once

namespace first_person::hybrid {
// Match the painter's interleaved sixteen-row strips. This schedules work only;
// it cannot alter triangle coverage, scanline arithmetic or submission order.
inline unsigned raster_worker_mask(int low,int high,int workers){
    const int first=low/16,last=high/16;
    if(last-first+1>=workers)return (1u<<workers)-1;
    unsigned mask=0;for(int strip=first;strip<=last;++strip)mask|=1u<<(strip%workers);
    return mask;
}
}
