# Source-owned installer file matrix

Full run39206891990/1040 completed the game processes but the final verifier
rejected its35-file payload using an obsolete hard-coded32-file count. The new
contract enumerates the required runtime/module/persistent-data files and adds
the two held-resource records and NVTX license when owned by the exact Git source.
It checks both manifest keys and actual ZIP members, retaining every SHA check.

Five negative tests reject missing licenses/manifests, removed module/data files,
same-count substitutions and undeclared ZIP members. The existing frame-phase
checks remain unchanged. Offline rechecks accept both actual f15735-file packages
and both32-file PSX/main references; that recheck does not replace fresh CI.
