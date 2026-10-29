---
id: 45
title: Gameplay repeats a picture about every 20 fields where the console runs at a full 60 fps
status: open
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

The repeats are the retail gameplay task `0x8001DAF8` running out of its turn budget: each one logs `needed more than one
display field of guest CPU and was RESUMED` (199 in a 30,000-present run), and the parked frame presents a field late
(`game/execution/bios_threads.cpp:Service::runGuestEntry`). The budget is `psx::cpu::ExecutionBudget::currentTurn`, one
field of 566,206 Lightrec cycles, and Lightrec charges a flat 2 cycles per instruction (psxport
`runtime/cpu/segment_clock.h`), so a turn holds 283,103 instructions. Demo 2's gameplay turns take 202K-566K Lightrec
cycles (median 364K; a temporary per-turn log, `scratch/readme-media/inv3/run.log`); the ones above 566K are the
repeats. The console finishes the same frames inside a field, so X4's real cost is below 2 cycles per instruction there.

## Proper fix

The guest CPU cost model psxport issue 0007 defers: per-opcode, memory wait-state and GTE stall costs in the one
`EmulatedTime` owner, with the turn budget in the same units, calibrated against the console oracle's cycle timestamps
across titles. This span (demo 2, 240-640 fields in) is an X4 calibration case: the console shows no repeat. A
different flat factor would only move which frames drop.

## Captures held back

The README carries no gameplay clip until this is fixed (2026-10-29 README media pass).
