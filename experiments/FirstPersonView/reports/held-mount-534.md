# Actual calibrated held models and accessory contacts

Module source `5348098618812df3af48f0d93d6ebb0577180466`; independently
verified Native 1807 Host source `322bab861f026e2a8222ac1b462bcaf31aacca6d`.
This local diagnostic mixed those explicitly separate identities and is not an
installer or unchanged final qualification.

- Host SHA-256: `2770917aad483178f2b77b644d235f57054b953c59fe74235eab142d8222e90e`
- Module SHA-256: `0b78fa667daa27f70e7bdf6f0fda0bbcd5266e9bad2b56aaddf9ae1f780e3def`
- Gallery manifest SHA-256: `a80fe90cbd87b21443f51563ebd11b1cc173a24104a89a3e9181c589e0cdfeb1`

Actual Native gallery: 64 cases, 32 CPU and 32 D3D12 on RTX4090, all 30 installed
model identities selected without fallback. Clean process exit, no crash dumps.
Warm static builds, static mesh uploads and dynamic geometry bytes were zero;
GPU resource peak was 31,406,672 bytes. Palette poses and actual ammo visibility
remain separately verified. All CPU/GPU contact sheets and the four accessory
before/after comparisons were inspected: the foregrip mounts below the rail and
the support hand follows it; the suppressor extends ahead of the muzzle.

In CI 39169451299 both Native pins passed their separate 64-case gallery. The
full gameplay processes subsequently completed, but the old outer verifier
rejected the newly added `native_held_assets_and_shared_psx_contacts` action
because its exact action set had not been updated. The new verifier requires
that action and independently validates its complete 64-case gallery; no action
is removed or accepted merely because the Native process exited successfully.
