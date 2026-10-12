#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace first_person {
// Frame invalidation, not an integrity receipt. Read unaligned scene bytes in
// bounded words; no callback-scoped pointer is kept after hashing.
inline uint64_t hash_bytes(uint64_t h,const void *data,size_t size){
    const auto *p=static_cast<const unsigned char *>(data);
    while(size>=sizeof(uint64_t)){
        uint64_t word;std::memcpy(&word,p,sizeof(word));
        h=(h^word)*1099511628211ull;h^=h>>32;
        p+=sizeof(word);size-=sizeof(word);
    }
    while(size--){h=(h^*p++)*1099511628211ull;}
    return h;
}
}
