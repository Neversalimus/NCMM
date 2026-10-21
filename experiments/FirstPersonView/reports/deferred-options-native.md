# Restore serialized typed options after the canonical repair layer

Actual Linux Native CI 39176744791 compiled the private bridge and reached
329/503 Native translation units. Both pins then failed strict compilation of
`options.cpp`: `ncmm_apply_deferred_option_value` was defined without callers.

The isolated full canonical preparation of pinned Native 1807 confirmed the
cause on actual generated source. The stale world-option repair layer replaces
all four typed registration bodies after the earlier persistence layer added
deferred-value restoration. The replacement drops all eight restoration calls.
This is also a Windows persistence problem; suppressing the Linux warning would
leave the serialized values unapplied.

The private experimental applier restores the original deferred application on
both creation and refresh paths for bool/int/float/enum, after repairing their
type metadata and before updating current-world copies. It requires the exact
final canonical bodies before writing any file. Canonical payloads, public ABI
and shipping main remain unchanged.

Native qualification now seeds four nondefault values in the fresh test game's
actual `config/options.json`. The real Native process must deserialize them,
register all four types twice and retain all eight observed values. The fixture
runs during the existing Windows save/restart gate and Linux controls process;
it is inactive during ordinary rendering and performance measurements. Actual
Native runtime results remain required before accepting this repair.

The benchmark also guards the removed legacy `FOV_3D_Z_RANGE` option before
reading it and records the actual loaded Z bounds/slice count. Both pinned
engines removed that legacy option; telemetry must not manufacture an unknown
option just to describe the unchanged loaded-world window.
