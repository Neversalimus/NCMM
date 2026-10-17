Drillimpact PSX First Person Arms 1.1.0 / CC0-1.0
================================================

Source: https://drillimpact.itch.io/psx-first-person-arms-free
Archive SHA256:
13689348acce167599db2b7190214aa4dbe914f5547f74e1877ff61361fc8cb8

Regenerate from the exact official archive, offline:
  python -m pip install Pillow==11.3.0
  python tools/import_psx_arms.py --archive PATH_TO_AUTHOR_ZIP --output assets/psx_arms
Run these commands from experiments/FirstPersonView. The converter never
opens Blender or executes code from the archive. Other archive versions
are rejected until explicitly audited and pinned.

model.generated.cpp holds typed immutable vertices, normals, UVs, the full
joint hierarchy, inverse bind matrices, original weights and animation keys.
Only constant animation tracks are compacted, without resampling. Vertices
on a hand/forearm material boundary are duplicated with identical geometry
and weights. No influences are discarded or rounded to a dominant bone.

bare.rgba and gloves.rgba are unscaled 512x512 RGBA8 pixels, in R,G,B,A byte
order. CMake verifies their hashes and generates typed uint32_t texture words
in a separate compiled translation unit. Runtime never parses GLB or PNG and
does not require Python, network access, an additional graphics device or a
second rendering pipeline.

Weighted palette entries are weighted sums of the original joint skinning
matrices. The existing single-joint GPU vertex format then evaluates the
same linear blend skinning as the source GLB, including all four influences.
Texture variants select real clothing state; black gloves are not the default.
See the repository THIRD_PARTY_NOTICES.txt and manifest.json for provenance.
