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

## User reports 2026-11-12

Bug reports `20261112-153948` (stage start: left margin black past layer 0's map edge), `-154058` (a boss cut
vertically at canvas x 58, guest x -16, the first retail tile column), `-154132` and `-154209` (black margin
blocks), `-154304` (boss room: the margins past the room walls). User decision: a margin that cannot be filled is
shifted or clamped in host presentation only (guest camera and state stay retail), otherwise pillarboxed; rendering
the area for real is preferred.

Leads, not yet proven on those screens:

- Retail `0x80027344` refills ring strips of every layer looping between map columns `+0x4D` and `+0x4E` of the
  layer's camera; `refillRing` writes 0 past the map instead. Looping parallax margins the same way changed no
  frame of the stage-start recording (pad frames 2706-3400), so it is not yet shown to own any reported screen.
- The camera bounds are `+0x1C..+0x22` of each layer (`CameraLayerOffsets`); a clamp or pillarbox keys on them.
- The reports' pad recordings stop reproducing their screens on HEAD: the widened placement scans
  (`activity_window.*`) change enemy timing, and `PSXPORT_LOAD_STATE` crashes (issue 0046), so these screens need
  a warp to their room first.

Reproduced on HEAD (2026-11-12): report `-153948`'s pad to frame 2704 (`scratch/bugs-1112/m4`). Layer 0 camera
x 40, left bound 0 (`+0x1C..+0x22` read 2976, 0, 256, 256), margin 74 at the 3840x1975 sink, so the left 34 px of
the margin lie before map column 0. `warp 0/1` lands after the intro boss (`scratch/bugs-1112/w01`), so the
boss room of reports `-154058` and `-154304` needs a part-0 checkpoint arrival in `StageWarp`.

Design for the map-edge and room-wall cases (reviewed with Fable): the margin becomes `{left, right}` per frame.
`psx::present::guestProjectionPlan` takes each side's fillable extent from a title policy beside
`presentationAspect` (X4: layer 0 camera x against its bounds) and clamps it to `[0, 2M]` with
`left + right <= 2M`. The plan's scalar `presentationHorizontalMargin` goes away. The record canvas is
`buffer + left + right` wide with the display buffer at its VRAM columns, so a camera at a bound shows the room's
far side instead of the void, and a room narrower than the wide view gives a narrower canvas, which the presenter
pillarboxes. The split is stamped into the `FrameRecord` at latch time so a record is presented with the split it
was drawn with. Consumers take per-side values: `WideBackground` columns, `HudAnchor::shiftPacket`,
`ScreenWindow::horizontal`, `activity_window` boxes, `RecordReadBack::captured`.

Map-edge fix (2026-11-12): `view_bounds::fillableMargins` limits each side to the gameplay camera's clamp window
(layer 0 `+0x1E` min x, `+0x1C` max x, the words `FUN_80028BF0` copies from the room entry) and to every enabled
layer's map (no tile left of column 0 or past block `+0x4E`). The other side takes the rest of the 2M columns
(`WidescreenPolicy::presentedMargins`). Every margin consumer reads `WidescreenController::margins`. Report
`-153948` at pad frame 2704: before, 34 black columns before map column 0 (`scratch/bugs-1112/m4`); after, the canvas
holds 40 columns left and 108 right, with no black column (`m4c/shot_2704.png`). Report `-154132`'s pad from 2706 to
2994, every 6th frame, shows no black column (`m4d`).

Still open:
- At the stage start the left limit comes from layer 2, which scrolls at half the camera's speed. The 4:3 window
  then moves on screen by whole pixels at that rate. Motion across consecutive presents is not yet judged.
- The boss-room pillarbox (`-154304`) and the cut boss (`-154058`) need the part-0 checkpoint arrival.
- Arena capacity (`-154132`, `-154209`) is unchanged; a wider right side draws more margin sprites against the
  1,000 cap.

Motion judged (2026-11-13, `scratch/bugs-1112/mot`, every frame, report `-154132`'s pad): X4 presents no
in-betweens (`[playcheck] records: in_betweens=0`), so each present carries its own frame's split. With the
left side bounded by layer 2 (`scroll = round(camera / 2)`), the camera scroll 40 -> 153 at 2 px/frame (pad frames
3925-4000) moves the floor 1 px/frame, then 1,2,1,1,2 near x 148 where the bound hands back to the margin. The walk
40 -> 22 (3054-3066) moves it -2,-1,-1,-1,-2,-1,-1,0. That is judder, so `2f699c7` must not ship as is.

A uniform presentation shift cannot keep the layers' parallax. Bounded by the camera window alone, the world would
stand still near a bound while layer 2 drifts the other way at half the camera's speed. The fix that keeps both
is a host view camera: inside the shift zone every layer is drawn as at the clamped camera, so retail's layer
packets move by `s - (scroll_l(v) - scroll_l(x))` where `v` is the clamped camera. That needs each layer's scroll
rule from its mode function (`kCameraModeFnTable` 0x800F3134; layer 2 is mode 4) recovered, not a measured ratio.
A layer past its map end then shows retail's own wrap (`FUN_80027344`: a block below `+0x4D` maps to
`+0x4E`'s side and back).
