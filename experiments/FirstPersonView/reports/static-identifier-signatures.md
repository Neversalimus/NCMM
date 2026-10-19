# Hash semantic renderer identifiers instead of unused array padding

The full Native hardware matrix attributes roughly 1–3.6 ms to GPU geometry
preparation in warmed camera frames. Its static signature hashes six fixed
128-byte identifier arrays per source cell, including all unused bytes after
their terminating NUL. Typical Native identifiers are short and several slots
are empty. Those 768 bytes per cell are mostly unrelated to rendered content.

The candidate retains the original normalized numeric facts, world coordinates,
permissions, neighbour signatures and resource/tileset epochs. It hashes each
bounded renderer identifier with an explicit length prefix and its actual bytes.
Compile-time layout assertions require review if either cell structure grows;
even an invisible nonterminated array is read only within its declared bound.
No geometry variants or additional retained scene data are introduced.

The regression test modifies only bytes following each identifier's first NUL.
The old implementation rebuilds static cells although rendered IDs are identical.
The candidate reuses them and compares every static cell/pass mesh identity with
an independent fresh collector. Real identifier changes and physical floor
permission changes still invalidate the affected geometry.

The Native before/after timing comparison, memory and tail-latency checks remain
pending. Unit correctness and fewer hash bytes do not establish a frame-time win.
