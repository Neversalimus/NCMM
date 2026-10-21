# Aim at actual flying target pixels

Native 1040 and 1807 run 39169500278 reached the new flying-eye CPU case after
the preceding 18 target cases. CDDA reported the eye visible, while the central
pixel correctly had no target. The preserved actual 640x480 window capture
shows the crosshair below and left of the opaque iris. Using the ground-zombie
camera angle was an invalid expected-selection fixture.

The fixture now aims into the authored iris (yaw offset +0.06, pitch -0.03) and
resets the original angle before the NPC cases. It retains the same real Native
creature, its actual HP, per-pixel selection and the strict identity/visibility
assertions. It does not turn the transparent sprite rectangle into a hit area.
The corrected flying/wide/GPU cases still require a new Native run.

Evidence: `first-person-runtime-evidence-1040-graphics`, artifact 12152378729,
whole ZIP SHA256 `ceab9e1c468dfefc47ac08bc49353491c85545b8792a6e261893fe2d7c343534`.
Its full digest and every extracted member were verified locally. The unexpected
capture is `target-gallery/first-person-audit-target-hud-unexpected-flying-cpu.bmp`;
the runtime log records `native_visible=1 expected=1 actual=0`.

The subsequent walking step did not exercise Native walking: UDP checkout had
been skipped after the earlier failure. That read-only resource checkout now
runs whenever installer preparation succeeded, allowing independent diagnostics
to continue. The target failure still fails overall qualification. Target
captures and process receipts are uploaded immediately, including on failure.
