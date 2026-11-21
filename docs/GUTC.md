# GutC compatibility pin

Reference revision: `dinilmusick/GutC@0e26ac721c7a020078cfaf52e6fce259fe03301e`.
Set as `KRNLC_GUTC_REFERENCE_SHA` in CMake.

`KrnlC` deliberately consumes **no GutC API** in this version: its generic
C11 bounded execution primitives need only the C standard library. Thus there
are no consumed GutC exports to verify and no claim of a working GutC adapter.
External adapters may use GutC at the pinned revision after independently
verifying the needed actual exports. If an integration is added, add an exact
revision check, capability tests and a real adapter before marking it supported.
Do not add placeholder success paths or change GutC to unblock KrnlC.
