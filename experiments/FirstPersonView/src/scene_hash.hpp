#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace first_person {
// Frame invalidation, not an integrity receipt. Read unaligned scene bytes in
// bounded words; no callback-scoped pointer is kept after hashing.
inline uint64_t hash_bytes(uint64_t h,const void *data,size_t size){
    const auto *p=static_cast<const unsigned char *>(data);
    if(size>=32){
        // Scene records are hundreds of bytes. Four independent streams avoid
        // one serialized multiply chain while retaining every input byte.
        uint64_t a=h,b=h^0x9e3779b185ebca87ull,c=~h,d=h+0xc2b2ae3d27d4eb4full;
        do{uint64_t words[4];std::memcpy(words,p,sizeof(words));
            a=(a^words[0])*1099511628211ull;a^=a>>32;
            b=(b^words[1])*1099511628211ull;b^=b>>32;
            c=(c^words[2])*1099511628211ull;c^=c>>32;
            d=(d^words[3])*1099511628211ull;d^=d>>32;
            p+=sizeof(words);size-=sizeof(words);
        }while(size>=32);
        h=((a<<7)|(a>>57))^((b<<19)|(b>>45))^((c<<37)|(c>>27))^d;
        h*=1099511628211ull;h^=h>>32;
    }
    while(size>=sizeof(uint64_t)){
        uint64_t word;std::memcpy(&word,p,sizeof(word));
        h=(h^word)*1099511628211ull;h^=h>>32;
        p+=sizeof(word);size-=sizeof(word);
    }
    while(size--){h=(h^*p++)*1099511628211ull;}
    return h;
}
}
