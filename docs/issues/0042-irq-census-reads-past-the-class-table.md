# 0042 — IRQ census reads past the class table

Status: closed (2026-10-27). Owner: `game/frame/vsync_sync.cpp` (`censusIrqTable`).

## Observed
Every run logged `[x4-vsync:error] IRQ class 11 handler at [0x8011CBC4] is 0x0000000D ... in NO loaded code image`,
and the same for class 15 (`0x80200000`), from field 0 on (the all-title real-time run of 2026-10-27).

## Cause
`censusIrqTable` scanned 16 class slots. libetc's dispatcher `0x800E5208` loops classes 0..10 only; the word after
the table, `0x8011CBC4`, is the enabled-class mask that `InterruptCallback` (`0x800E53F0`) writes, and the words past it
are other data. The census read them as handlers.

## Fix
`kIrqClassCount` is 11.

## Proof
`scratch/player-bugs/irq` (20 s from boot): `scanned 11 of 11 classes, 3 populated, 0 outside every code image` at every
census, no error lines.
