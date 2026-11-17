---
id: 48
title: With fast loading off the product aborts at boot in the retail CdControlB
status: open
symptom: `PSXPORT_X4_FASTWAIT=0` (and so any typed comparison run, which suppresses every enhancement) dies before the first present
tags: loading,cd,comparison
state_items: S007
created: 2026-11-17
updated: 2026-11-17
---

## Reproduction

Hidden, silent, unpaced, `aspect=0`, `ires=0`, sink 3840x2160, `PSXPORT_X4_FASTWAIT=0`, no input: about 0.4 s after
`[session] Mega Man X4 (SLUS_005.61) booted` the run logs

    [guest-call:error] fast_wait::CdControlB original: guest call 0x800E5FF4 to return address 0x80013874 refused
    after 8 host turn(s) and 4529682 cycles, stopped at 0x800E69F0: the call reached its turn cap

and the watchdog aborts (`callOriginalResumingToReturn` from `NativeDispatcher::invoke`). The same binary with the knob
on boots and plays (2026-11-17, ally, `scratch/gaps/load-off`).

## Cause

`game/execution/native_overrides.cpp:synchronousCdControlBlocking` hands any CdControlB outside a synchronous load to
`fast_wait::cd_control`, which then runs the retail body through `originalCdControlBlocking`, a scoped original call
with an eight-turn cap. Retail CdControlB is still waiting for the drive when the cap stops it (at `0x800E69F0`); its wait needs guest
fields to pass, and inside a native override none do. With the knob on, the boot's
CdControlB runs inside the synchronous loader's scope and is answered natively, so the defect only shows with it off.

## Proper fix

The retail blocking CdControlB must run where fields can pass: as guest code the frame driver resumes across fields,
not as a nested original call, or through the framework CD owner as the Pause command already is
(`cd_control_boundary::blocking`). Until then, fast loading cannot be compared against retail loading time and the
typed comparison runs that suppress it cannot boot.
