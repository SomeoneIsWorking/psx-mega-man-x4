---
id: 44
title: The 16:9 background margins drop tiles to black when the tile sprite arena is full and past a layer's map edge
status: open
symptom: In gameplay at 16:9, rectangular blocks or whole strips of the left and right margins are black while the 4:3 window is complete
tags: widescreen,background,render
state_items: S006
created: 2026-10-29
updated: 2026-10-29
---

## Reproduction

Hidden unpaced boot with no input, `aspect=1`, `ires=0`, `PSXPORT_PRESENT_SINK=1280x720`, present shots every 60
presents through both attract demos (`scratch/readme-media/explore3/`, sheets `d1a.png`, `d2.png`):

- Demo 1 (stage 2 part 1, Zero, starts about present 13,560): black margin blocks on most frames, e.g. presents
  13,980, 14,040, 14,100, 14,160, 14,220, 14,280, 14,340.
- Demo 2 (stage 0, X, starts about present 29,080): the left margin is black at 29,080-29,320 (a full-height strip
  and a lower block).

## Cause

Two separate gaps in `game/render/background_tiles.cpp:WideBackground::drawLayer`:

1. Arena capacity. Margin sprites share retail's tile sprite arena (`D_8015D9D0`, 1,024 SPRT_16 per buffer, retail
   stops at 1,000 @ `0x80026BB0`) and stop at that cap, reserving 336 sprites for each later layer. Retail alone
   already fills it: in demo 1 layer 2 starts its margin at count 973-1,000, and 1,280 cap hits were logged over
   ~860 presents (temporary log in `drawLayer`, `scratch/readme-media/inv1/run.log`). The arena is followed directly by
   the VRAM rect queue at `0x801659D0`, so it cannot grow in place.
2. Map edge. A margin column left of map column 0 (or right of `kMapRightBlock`) has no tile, so nothing is drawn
   there. Parallax layers scroll slower than the camera (demo 2 start: layer 0/1 at x 55, layer 2 at x 28 with a
   54-pixel margin), so their left margins run off the map even where no cap is hit. Retail never shows it at 4:3.

## Proper fix

1. Give the margin its own double-buffered sprite storage instead of retail's arena: either a guest RAM span proven
   unused for every stage (needs the heap map above BSS end `0x80175F38`: loads land at `0x80178000`, `0x801F3000`,
   `0x801F6000`, task records at `0x801F8100`), or a psxport record-path seam that lets a title append host-owned
   packets to an OT bucket. Size: 3 layers x 8 columns x 16 rows per buffer.
2. Decide the map-edge policy from the stage data (wrap of looping layers, or the layer's own edge block), measured
   per stage; until then the edge stays black.

## Captures held back

The README carries no 16:9 gameplay stills, comparison or clip until this is fixed (2026-10-29 README media pass).
