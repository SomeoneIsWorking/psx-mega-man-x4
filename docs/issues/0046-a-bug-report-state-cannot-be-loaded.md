---
id: 46
title: A bug report's machine state cannot be loaded
status: fixed
symptom: PSXPORT_LOAD_STATE with any bug report's state.bin dies within a few fields
tags: debug,state,threads
state_items: S006
created: 2026-11-12
updated: 2026-11-13
---

## Reproduction

`PSXPORT_LOAD_STATE=<report>/state.bin PSXPORT_PAD_SHOT_AT=30`, every report of 2026-11-12: report
`20261112-153842` faults in a guest call to `0x800DD7FC` (Lightrec fault at `0x80120000`, sp `0x800DD7D8`); report
`20261112-153948` aborts with `ChangeTh refused invalid transfer to handle 0xFF000002`.

## Cause

The state restores guest RAM and devices but not the host owners that hold guest execution: Mega Man X4 implements
no `GameRuntime::nativeState`, so `bios_threads::Service` comes back with no task fibers, parked registers or
UpdateTasks fiber, and the first ChangeTh names a task that does not exist on the host.

## Fix

Serialize `bios_threads::Service` (open slots, per-task registers and native nesting, budget turns) through
`nativeState`, and resume each task at its saved pc on a fresh fiber.

Done 2026-11-13: `x4::TitleState` adds the `x4.tasks` section. A task's turn loop keeps its continuation in its
`Thread` (`started`, `returnAddress`, `resumePc`, `retired`), so a task parked at the end of a turn
(`parkAtTurnEnd`) is fully described by those and its registers; `adoptStaged` drops the old fibers and the next
ChangeTh starts a fresh one that resumes at `resumePc`. A save refuses, by slot and pc, while UpdateTasks is parked or
a task is inside a multi-field VSync wait. States written before this have no task section and still cannot load,
so the 2026-11-12 reports stay unloadable.

Proof (`scratch/bugs-1112/st`): report `-154058`'s pad, `warp 0/0/1`, `state save` at frame 3000; a fresh process
with `PSXPORT_LOAD_STATE` logs `restored task slots` with slot 2 open, then plays 600 frames with X on the
checkpoint-1 highway (`st/sheet.png`: the source run 60 frames after the save, then the loaded run at +60 and +600).
