# Allow complete Windows Native qualification to finish

Historical Core9c Full39350962019 and CommonB19 Full39352183335 both have the GitHub annotation `The job has exceeded the maximum execution time of 4h30m0s`. B19 stops in the paired baseline phase before downstream image qualification. Core9c records individually successful steps but its overall job is cancelled; neither run is accepted as successful Full qualification.

Increase only the total Windows job allowance from270 to360minutes, within the documented six-hour GitHub-hosted job limit. Every individual test command, assertion, per-step deadline, source/installer check and final qualification gate remains byte-identical. Exact reversal recovers the entire previous workflow. Eight qualification-policy tests pass. Runtime, public ABI, SDK, shaders and visual quality are unchanged. This infrastructure fix supplies no performance or release acceptance.

Evidence: [Core9c job](https://github.com/Neversalimus/NCMM/actions/runs/39350962019/job/118398695395), [CommonB19 job](https://github.com/Neversalimus/NCMM/actions/runs/39352183335/job/118402607985). GitHub documents the [six-hour hosted job limit](https://docs.github.com/en/actions/reference/limits) and [job timeout setting](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#jobsjob_idtimeout-minutes).
