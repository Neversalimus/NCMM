# Main control: complete frame clock

Keep production Main f8affe5570b632178eb3c9ad9d103c40701e0eb8 unchanged. This
reference retains the previously qualified 2f84060 diagnostic harness and the
same pinned corrected SDL. Its only new Native change moves the existing GPU
metrics/readback query before the frame and process-CPU clocks stop, matching
the repaired PSX control. Rotation and action outer clocks already included it.
No renderer, asset, quality or public ABI changes are made. Missing newer PSX
counters remain unavailable. This is a new control requiring actual Native
comparison, not a reinterpretation of the preserved old measurements.
