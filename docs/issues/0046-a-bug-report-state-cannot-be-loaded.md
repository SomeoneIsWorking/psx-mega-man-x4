---
id: 46
title: A bug report's machine state cannot be loaded
status: open
symptom: PSXPORT_LOAD_STATE with any bug report's state.bin dies within a few fields
tags: debug,state,threads
state_items: S006
created: 2026-11-12
updated: 2026-11-12
---

## Reproduction

`PSXPORT_LOAD_STATE=<report>/state.bin PSXPORT_PAD_SHOT_AT=30`, every report of 2026-11-12: report
`20261112-153842` faults in a guest call to `0x800DD7FC` (Lightrec fault at `0x80120000`, sp `0x800DD7D8`); report
`20261112-153948` aborts with `ChangeTh refused invalid transfer to handle 0xFF000002`.

## Cause

The state restores guest RAM and devices but not the host owners that hold guest execution: Mega Man X4 implements
no `GameRuntime::nativeState`, so `bios_threads::Service` comes back with no task fibers, parked registers or
UpdateTasks fiber, and the first ChangeTh names a task that does not exist on the host.

## Proper fix

Serialize `bios_threads::Service` (open slots, per-task registers and native nesting, budget turns) through
`nativeState`, and resume each task at its saved pc on a fresh fiber.
