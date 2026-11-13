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

Framework defect under the split (2026-11-13, reproduced on HEAD): report `-154132`'s pad at judgement settings
(`scratch/bugs-1112/fw/head`), frames 3935-3936 and 3939-3945 showed the 4:3 window with black margins (left 21,
right 87 columns). psxport keyed canvases on `(left, right)`, so every split change recreated them black, and the
split was read from live guest state at present time: X4 builds its ordering table in frame N, `DrawOTag`s it in
N+1's prefix and seals it in N+2's field, so the latched split was a frame newer than the picture. Fixed in psxport
`58032b9c` (each `FrameRecord` carries the margins it drew; canvases are allocated at a 2M reach per side and keep the
window of the record that last drew them; titles answer `GuestWidescreenProjection::pictureBeyondBuffer` and
`present::fitMargins` makes the split) and here in `664af43` (`WidescreenController::submitFrame` at the frame
prefix's `DrawOTag`). After: no black column in any of those frames (`fw/head_3935.png` before, `fw/t0_3935.png`
after); Tomba 2 warp 13/0 and Spyro 1 warp 0/1/0 still render both margins (`fw/tomba2_13_0.png`,
`fw/spyro1_0_1_0.png`).

Host view camera (2026-11-13, RE in `scratch/bugs-1112/re/modes`):
- Layer 1 and 2 scroll x is a pure function of layer 0's x per mode (tables `0x800F3140`, `0x800F3164`, the same
  rules for both): 1 and 4 `x >> 1`, 2 `x`, 3 `x >> 2`, 6 `(x >> 1) + (x >> 2)`, 7 `x + (x >> 2)`, 8 `x + (x >> 1)`,
  plus the stage's `+0x40`; 5 (`FUN_800285E0`) leaves the layer to its owner. Stage 0 picks mode 2 / 4
  (`D_800F32D4`). The layer is then slew-limited against last frame's value (`FUN_80027FA8`/`FUN_80028450`: 16 px
  per frame, a jump past 0x148 folds by 0x140). That binds only on a camera jump; the shift
  `layerShift = s - (scroll(x + s) - scroll(x))` uses the pure part, which the limiter's state cancels in.
- Bounds: rooms set the target window `+0x24..+0x2A`; `FUN_80028690` walks the active window `+0x1C..+0x22` toward it
  at `+0x47` (2) per frame when it tightens on the camera and lets it go at once when it relaxes. The view therefore
  follows the camera's clamp at once, and after a relaxation returns at `+0x47` per frame beyond the camera's own
  motion, never against it (`view_bounds::nextView`).
- Loops: `FUN_8002771C` starts every layer on the whole map (`+0x4D` 0, `+0x4E` row blocks - 1); only stage objects
  (`FUN_800D8684`, `FUN_800D9268`, `FUN_800D9B48`, `FUN_800DA2D4`, `FUN_800DA8B4`) set other loop blocks, and
  `FUN_80027344` wraps a block past them into the loop (moving the layer's scroll by the span when it leaves it). A
  layer on its default blocks does not loop and limits the picture; a looping layer's margin reads the wrapped column.

Proof (`fw/t1`, `fw/t1b/view.txt`, same pad, every frame 3920-4000 and 3050-3068): no black column in any frame. The
view offset is 11 at camera x 43 and falls 2 per frame to 0 at x 55, then stays 0 (`view.txt`). Canvas motion: 0
through 3945 (view at the left bound), floor 1 / sky 0 for 3946-3973, floor 2 / sky 1 for 3974-3999 (`fw/t1_3930.png`,
`t1_3950.png`, `t1_3985.png`). In 3946-3973 the picture's left side grows one column per frame (16 -> 54) because
stage 0's far layer (mode 4, default loop blocks) runs out of map, so in view coordinates the floor moves 2 and the sky
1 per frame, steady. The walk-left section stays at the bound (0 / 0).

In-place pillarbox (2026-11-13): `pictureBeyondBuffer` now returns `PictureBeyondBuffer{view, picture}`. X4's view is
layer 0's clamp window (`{M - offset, M + offset}`, a room narrower than the wide view centred in it), its picture the
room and every non-looping layer's map. psxport `fitWindows` makes each record's `RecordWindows{view, drawn}`; the
canvas draws the drawn window and the present always shows the 2M-wide view window, black outside the drawn columns
(`present.frag` `cols`). Every X4 margin consumer (background columns, HUD anchor, cull, activity boxes) reads
`windows().drawn`; the HUD anchored to the view edge was cut by the strip. Proof (`scratch/bugs-1112/pb`, report
`-154132`'s pad, every frame 3936-3980, judgement settings): every in-stage present samples the same 3424-wide view
into the same viewport (`disp=…,3424,1920 viewport=0,3,3840,2153`); the left strip is 27 columns at the bound
(3936-3946), then shrinks 1 per frame to 0 at 3973 with the far layer, while the floor moves 2 and the sky 1 per frame
from 3947 to 3980 with no step at the handoff (`a_motion.txt`, `a_3940.png`, `a_3960.png`, `a_3980.png`,
`b_3940.png` with the HUD whole).

Layer-bound objects (2026-11-13, RE in `scratch/bugs-1112/re/objdraw2`, `objdraw3`): the draw pass `0x80023DB8` places
objects through four draws that subtract the scroll of the layer named by the object's s8 background byte: the sprite
builder `0x80024334` and X's armor parts `0x8002588C` (BaseObj `+0x14`), and the quad draws `0x80024920` / `0x80024B9C`
(QuadObj `+0x37`, chosen by bit 1 of the object's first byte). All four write into the scratchpad arenas `0x1F800100`
/ `0x1F800104`. `layer_objects` wraps each: the packets written during the call move by `camera_layers::shiftOf` of
that layer, the same shift `WideBackground::drawLayer` gives the layer's tiles; `VisibilityCull` adds it to the
object's screen x (`presentedCull`). On report `-154132`'s pad the layer-2 objects are quad pool slots 0-2
(`0x801435B0`, `+0x60`, `+0xC0`), the stage 0 searchlight beams, with shifts 7 -> 0 over 3920-3941. A/B at pad frames
3930, 3936 and 3941 (`pb/ab_*.png`, top without the override, bottom with it): only beam pixels differ, by 7, 6 and 4
columns, and the sky is identical, so the beams now hold their place on layer 2.

Still open:
- Reports `-154058` and `-154304` are not reproduced. Their `state.bin` RAM (engine `0x801721C0`, camera
  `0x801419BA`) puts them in stage 0 part 0 at checkpoint 1, camera x 2976, and checkpoint 2, camera x 4848.
  `warp 0/0/1` and `0/0/2` arrive at x 1360 and 3328 (room entries `0x800F3338`, `0x800F335C`), so both rooms are
  a walk past the arrival, which needs steering. The states predate the task section (issue 0046) and cannot load;
  a report filed in those rooms on this build loads.

Arena capacity fixed (2026-11-13, RE in `scratch/arena/d1`..`d9`): the margin no longer touches retail's arena,
ring or chains. `WideBackground::drawLayer` builds each margin tile's SPRT_16 words (code `0x7D`, bit 1 for
semi-transparency, as every one of the 2,048 arena slots holds in report `-154132`'s state) from the block map
into a `MarginBatch` by side and texture-page bucket. After the layer's splice `0x80026894` (which puts each
non-empty group's texture-page node `0x80171EB0` in front of its sprites and links the group in front of its
ordering-table slot, `drawInfo + 0x70 + depth * 4`, depth at camera layer `+0x4A`/`+0x4B`), `spliceMargins`
inserts them as psxport `HostPackets` into the walk from `drawInfo + 0x9C`: a group retail drew goes before its
tail's next node (checked to be the slot's previous next), and a group retail left empty goes, with a copy of its
texture-page node, before the node the splice would have put it in front of. The frame driver discards the table
it clears. The ring refill override and the 1,000-sprite budget are gone.

Found on the way: at the `0/0/2` arrival the walk skipped slots 5..3 (chain `S6 -> 0x8016BD98 -> 0x801539C8 -> S2`).
A widened-only object (`0x80173D00`, id `0x12`, raised by `cull::objectDrawPass`) reached the sprite builder
`0x80024334` with a frame of 0 pieces; with none, the builder still links its group tail to the unwritten draw-mode
node at `0x1F800104` and makes the packet before the cursor the tail, so the next quad draw reused that node in a
second group and `0x80024260` spliced it into two slots. `layer_objects` now draws nothing for a frame of no
pieces. The dark right-margin column at that arrival is the room's own door gap: the 4:3 run shows it at the same
place (`scratch/arena/a0_sheet.png`, `a3_sheet.png`).

Proof (`scratch/arena`, report `-154132`'s pad, `warp 0/0/2`, judgement settings):
- Frames 2760-3000 every 60 (`w3_sheet.png`): no black block in either margin. `PSXPORT_DEBUG=x4-bg` logs every
  layer's margin (up to 96 sprites, retail up to 529) and groups retail left empty filled (`2 in 1 group`), and no
  walk ended with an undrawn host packet (a fault otherwise).
- Guest RAM: the layer shift of retail tiles, the layer shift of object pieces and the HUD anchor no longer write
  guest packets; each names the packet with `HostPackets::moveX` and the walk draws it moved. A state at frame 3000
  (`w3/A.bin`) loaded at 16:9 and at 4:3, main RAM and scratchpad snapshotted each of the next 30 frames and
  paired by the retail field counter `0x80141BD8` (`ramproof2.py`, `m3/`, `m0/`): 7 addresses differ, all the
  activity window. The placement table entries at `0x800F4AD0` and `0x800F4AD8` (spawned byte, placements at
  x `0x0E50`, y `0x01B0`) and the object at `0x80142FC0` hold 1 / live at 16:9; at 4:3 the lifetime check
  `0x8002B160`, called from the object's update at `0x800B57AC`, finds the object outside the retail box and clears
  both spawned bytes on the first frame (store watch, `wl0/`). The same frame at 16:9 has no store: the widened box
  keeps it. This is the activity window doing what `activity_window.*` is for (an enemy placed before it enters the
  margin and retired where the widened view ends), so it stays: gameplay at a wide aspect differs from 4:3 only by
  which placements are live.
- A load draws what the save would have drawn next: the machine state carries the host packets (psxport GPU
  section) and the wide view (`x4.tasks` v2; the view eases toward its bounds, so the camera alone does not give it
  back). From `w5/A.bin`, the first walk after the load draws the 9 restored host packets and the moved tiles
  (`otattr`), and its record has the HUD at the edge and the left margin drawn as in the saving run (`l11/`).
- The same psxport build draws Tomba 2's margins at `warp 13/0` identically before and after the walk change (0
  differing pixels, `scratch/arena/t2/`) and Spyro 1's widened intro scene (`scratch/arena/s1b/cmp.png`).

Still open:
- The first two records after a load draw no column past x 320 (the right margin is black), though the restored view
  and submitted picture are right (`l12/run.log`); from the third record on it is drawn. The loading run announces
  `native picture ... native_width=256` before the load and `320` on the first frame after it. Not traced yet.
- Co-op player 2's gauge (`coop/player_two_hud.cpp`) still moves its packets by guest writes.
