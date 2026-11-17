---
id: 47
title: The stage intro (READY, beam-in) has no guest route to its end, so Start cannot skip it
status: open
symptom: Every stage arrival shows READY and the beam-in for about 200 fields before the player has control; Start does nothing there
tags: skip,sequence,stage
state_items: S011
created: 2026-11-17
updated: 2026-11-17
---

## Reproduction

Any stage arrival (`warp <stage>` from a running stage, or the pad route into stage 1): engine state 6 sub-state 0,
player state `0` for about 90 fields while READY plays, a 24-field drop, then `0x201` about 100 fields later.

## Cause

READY is a misc object (`MegamanRelatedUpdate`, `external/mmx4/src/main/ready_text.c`, from `0x800CB048`) that
runs its palette cycle and stay-up timer to completion and then releases the player object, whose beam-in runs on
its own timers. None of the READY states is among the readers of `controller_state` (`0x80166C0C`) or the held word
(`0x80166C08`) (Ghidra `--refs`, 2026-11-17), so retail has no skip route there and no terminal transition a skip
could take.

## What a fix would need

The only way to shorten it is to advance the READY object and the player's beam-in states to their end states, which
is manufacturing guest state, not taking a guest transition (workspace `AGENTS.md`, Title scope: a screen with no
faithful terminal transition stays unskippable). It stays as retail plays it.
