#pragma once
#include <set>
#include <utility>

// Native support-cache coordinate translation, with its default std::allocator.
// The independent oracle remains the complete pinned map::shift body.
namespace ncmm_graphics_support_cache {
template<class Position, class Offset>
void rebase( std::set<Position> &positions, const Offset &offset )
{
    if( positions.empty() ) return;
    std::set<Position> previous = std::move( positions );
    positions.clear();
    // Detach before changing a key, and drain the separate old set so an
    // inserted translated position can never be processed a second time.
    // Both sets use std::allocator, which permits transferring their nodes.
    while( !previous.empty() ) {
        auto node = previous.extract( previous.begin() );
        node.value() = node.value() + offset;
        // Translation preserves Native coordinate order. Other valid orders
        // remain correct: std::set falls back when the hint is unhelpful.
        positions.insert( positions.end(), std::move( node ) );
    }
}
}
