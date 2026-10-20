"""Keep the pinned Native predicate equivalent under GCC 13's strict build.

GCC's inlined std::any_of copies the optional-backed vehicle part iterator and
reports an uninitialized vehicle pointer. Walk the same range directly, with the
same short circuit and predicate. No diagnostic is suppressed.
"""

BEFORE = '''    bool allow = !state ||
    std::any_of( found.begin(), found.end(), [&here]( const vpart_reference & vp ) {
        return vp.vehicle().can_enable( here, vp.part() );
    } );'''

AFTER = '''    bool allow = !state;
    if( state ) {
        for( const vpart_reference &vp : found ) {
            if( vp.vehicle().can_enable( here, vp.part() ) ) {
                allow = true;
                break;
            }
        }
    }'''


def prepare(source):
    if source.count(BEFORE) != 1:
        raise ValueError('Pinned Native electronic-toggle predicate changed')
    return source.replace(BEFORE, AFTER, 1)
