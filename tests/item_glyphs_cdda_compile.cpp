#include "item.h"
#include "item_category.h"
#include "itype.h"
#include "ncmm_item_glyphs.h"

// Compile the shared classifier against the actual target headers, not a fixture.
std::string ncmm_item_glyphs_compile_check( const item &it )
{
    return ncmm::item_glyphs::symbol( it, true );
}
