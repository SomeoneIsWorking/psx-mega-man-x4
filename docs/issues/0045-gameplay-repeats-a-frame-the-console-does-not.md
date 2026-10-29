---
id: 45
title: Gameplay repeats a picture about every 20 fields where the console runs at a full 60 fps
status: fixed
symptom: In the attract demos about one present in twenty repeats the previous one (a one-field hitch, ~3 per second); the Beetle console reference shows no repeat over the same demo
tags: timing,frame-loop,cpu-cost,psxport
state_items: S003
created: 2026-10-29
updated: 2026-10-29
---

## Reproduction

Hidden unpaced boot with no input, `aspect=0`, `ires=0`, every present from 29,380 to 29,640 shot
(`scratch/readme-media/clip2/`): attract demo 2 (stage 0, X on the highway). 13 of 260 presents equal the one before
(29,383, 29,404, 29,426, 29,445, 29,468, ...).

Console reference (psxport `tools/oracle/console.py`, OpenBIOS, `scratch/readme-media/tools/oracle_lag.py`): the
same demo from 240 to 640 fields after it starts, 400 fields, 0 repeated pictures (`scratch/readme-media/oracle/lag.out`).

## Cause

`x4::bios_threads::Service::change` (`game/execution/bios_threads.cpp`) answered the gameplay task's per-frame
`ChangeTh(main)` (`func_800127C8`, called at the end of every `0x8001DAF8` loop) by yielding the task fiber from
inside the ChangeTh callback. The executor call that ran the task never returned, so the next frame resumed inside
the same call and its one-field budget (566,206 instructions) accumulated across every frame since the last field
wait. Each time the total passed a field, a spurious `BudgetExhausted` came back, `runGuestEntry` logged `RESUMED` and
parked the task for a field: a repeated picture every 20-30 fields. Per-turn counts (temporary `x4-turn` log,
`scratch/filed-defects/0045/turns.txt`): 174 exhaustions in 30,000 steps, all at 566,2xx instructions with diffuse
PCs, and no frame-boundary turn logged after gameplay began. The console oracle runs the same demo at about 300K
instructions per frame including the main loop (`oracle_turns.py`, VSync(0) to VSync(0)).

## Fix

`Service::change` now ends the task's turn with a typed `CooperativeYield` exit at the ChangeTh return address;
`runGuestEntry` parks the fiber there like a field wait, and the next turn starts a fresh budget. A task that closed
itself (`func_800127FC`) parks once the same way.

## Shown

Hidden unpaced boot, no input, `ires=0`, sink 3840x2160: 3 budget resumes in 30,000 steps (was 174), all at
0x800264D0 in load-time turns; the census reads 23,798 field-boundary turns, 6,447 ChangeTh turns. Attract demo 2
(engine 5, stage 0) from step 29,381: `preseq 240`, 0 presents equal the one before (13 of 260 before the fix). The
console oracle: 0 repeats over 400 fields of the same demo.
