# Skip Native eligibility for inactive camera snapshots

Before this change, every selected non-actor cell ran the Native vehicle lookup before asking the camera snapshot to store it. When the snapshot was suspended or disabled, store() immediately returned null. First movement/easing frames and disabled-component controls therefore paid for a lookup which could not contribute cached data.

The private store_if helper checks snapshot activity before invoking the existing eligibility predicate. With an active snapshot, the same actor/death/vehicle predicate, exact cell/light copy, reuse, visibility verification and storage limits remain. No Native perception/animation pass, range, Z, resolution, public ABI, resource-retirement policy or SDL dependency changes. This branch starts at the production candidate 79d058d742aae71cf2baadd1ec4ca211b9c7ea06 and contains no CPU-cycle observer.

Local verification: 5261 snapshot ownership/lifetime checks pass on MSVC19.38 and GCC16.2. The new cases require zero eligibility calls while disabled/suspended/cleared, preserve retained data, reject dynamic eligibility and keep active-path checks. Replaying the former eager eligibility order fails the inactive-query assertion (exit1); the fixed implementation passes (exit0). Atomic Native source-transform checks are also run before publication.

This is an unaccepted candidate. It is a concrete avoidable query, not proof that it explains every timing regression. Fresh ordinary measurements and Native CI remain required, including active camera-cache behavior. Existing negative results are preserved; no 15 ms or merge acceptance follows from unit tests.

The preceding six instrumented CPU controls contain 4224 first-render and 12672 camera observations. They report raw current-thread cycles, not CPU elapsed milliseconds, GPU execution or a complete scheduler trace. Their verdict is diagnostic only: Main->PSX8 repeated metrics, Main->CommonOff8, PSX->CommonOff17. Original ordinary D6 rejection29 and prior ordinary core results remain unchanged.

- CPU diagnostic result SHA256: `524d2957fb1592eb4f297123bb46c7813103eca8896ab9917a924c84354e1714`.
- Complete local CPU report SHA256: `70a4e3162fd1bd4858c04afeab11defd011533771d619f5831cc399910cd17b7`.
- Raw CPU data remains in local `C:/workspace/fpv013-cycles-741-*` evidence folders; it is not claimed to be a downloadable CI artifact.
