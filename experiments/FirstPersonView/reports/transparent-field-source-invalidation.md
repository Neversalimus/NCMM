# Transparent fields and source-cache correctness

Native `map::on_field_modified` dirties transparency only for field types that
request it or are nontransparent. Its default `dirty_transparency_cache` is
false. Consequently, a transparent field can change the private renderable
source fact without reaching the existing graphics transparency notification.
The source cache must receive this change explicitly; cache-only reset scopes
do not establish coverage of field mutations.

The bridge now adds a one-cell absolute-coordinate notification to the
canonical Native field notification. Existing intensity updates, additions,
decay and removals in `process_fields_in_submap` already use that notification.
All original Native statements, cache flags, field processing and visibility
behavior remain intact. There is no field-processing replacement or per-draw
field observer. Common's region handler also ends the camera snapshot lease.

Both pinned map sources were transformed by the actual bridge. A GCC harness
executes each actual old/new notification body with the real source-cache
class: 193 checks pass per pin, including eight old transparent-field negative
controls that retain stale facts. Original Native call counts and field-cache
flags match. These are local policy tests with Native collaborators stubbed;
they do not claim an actual engine run. All 12 atomic bridge tests pass.

The short Native movement qualification now warms one real cached cell and
compares it with the unchanged Native source reader after six operations:
adding blood, changing its intensity, deleting it, adding smoke, changing its
intensity and clearing the cell. It restores the empty cell. This runs once
per short record, after a settled real step. The short 12-record qualification
must therefore verify 72 field mutation cases on the actual engine.
Extended ordinary timings execute no mutation probes and explicitly report
`field_mutations_oracle_checked=false` and zero cases. Their source-fact
oracle also stays disabled. Fresh exact binaries, Native probes, balanced
ordinary measurements and final Common qualification remain required.

The not-yet-fetched source candidate `248fbca` is superseded for hardware
qualification. Its controller was stopped before any Native launch. Earlier
negative results and raw evidence remain preserved; no performance acceptance
or 15 ms claim follows from the correctness tests.
