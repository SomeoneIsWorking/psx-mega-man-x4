# 0046 — Drop-in/drop-out X + Zero co-op

## The request

"Drop-in/drop-out two-player co-op: X and Zero on screen together, player 2 joins by pressing Start on pad 2 at
any point in a stage and leaves the same way, player 1's game unaffected while nobody joins" (G004). Added
2026-11-11: local co-op with a choice of a unified camera (one shared view) or split screen (each player their own
view), and LAN play with a choice of a unified camera or "everyone free" (each peer has its own independent
camera and screen). There is no server: LAN play is serverless, peers find each other on the local network or
connect by a typed host IP and port, and one peer hosts. The camera mode is a player setting through the config
owner.

## Approach, and where it comes from

The method is MegaManX4Recomp's (`docs/prior-art.md`, its own MIT code; nothing from `psxrecomp-v4`):

- **Projection.** The engine has one player. Player 2 is a private copy of every per-player guest block, swapped
  into the singleton addresses for a scoped pass and swapped back after it. The world (enemies, items, stage
  scripts, camera) still updates once per field against player 1; player 2's controller, vehicle, shots, trails,
  terrain collision and graphics decompression run in their own pass with player 2 projected.
- **Shared collisions retried per player.** An enemy-contact or enemy-hit test runs against player 1, and when it
  reports nothing it is run again with player 2 projected, so one consumable attack still hits once.
- **Unified camera.** The retail follow helpers read the player's position; for the shared view they read the
  midpoint of the two living players, and a leash undoes a frame's outward movement that would put the players
  more than one screen apart. With one player alive the camera follows that one.
- **The other hunter.** Player 1's campaign character picks player 2's: X's campaign gets Zero, Zero's gets X. The
  character code and animation tables of both are resident in the executable (`0x80119DF0` X, `0x8011AFF0`
  Zero); only the per-character archive (`ARC/PL00_U.ARC` X, `ARC/PL01_U.ARC` Zero) and the stage's character
  palette (`ARC/COLss_pc.ARC`) are not, so the title reads them from the player's disc.

What this port does differently, and why:

- MegaManX4Recomp allocates extra guest memory for the second character's archive and draws player 2 through
  host-side texture banks. psxport has the retail 2 MB and the guest's own GPU picture is the product
  (`AGENTS.md`), so: the counterpart's colors, effects, assembly and portrait members (0, 6, 7, 9, 15) are lent
  into the resident character's member slots only while player 2 is projected for its update, spawn and draw
  (each is no larger than the resident one); its body (member 2, larger than the resident's) is never resident:
  each animation change lends the one frame's compressed stream as a one-entry blob at the start of the resident
  body, the guest's own decompressor `0x80016FF4` runs on it, and the resident bytes go back. The decompressed
  tiles go to the two tile rows below player 1's (VRAM `0x140,0x20`), the stage's counterpart palette
  (`ARC/COLss_pZ.ARC`) to the eight CLUT rows below the character rows (VRAM `0,488`), and player 2's sprite
  packets are the guest builder's own output with its tile and CLUT words moved there for the call.
- Its join and leave are Select-hold gestures and automatic enrollment at stage start. This port follows the
  request: an edge of Start on pad 2 joins and leaves. The retail router already decodes pad 2 into
  `0x80166D50/52/54` every field and nothing reads it, so the edge is the game's own decode, and pad 2 is
  delivered through psxport's input layer (two controller ports).
- Its extended lifecycle (shared retries, beam-in/out, menus, doors, Ride Chasers, bosses) is a later phase here.

## Determinism and single-player parity

With co-op off, or on with nobody joined, every co-op override calls the retail body and returns: no guest byte
differs from a build without the feature. Evidence is a pad replay of the same session through both builds with
the same guest RAM at the same fields. Comparison runs suppress the knob (`psx::config::enh`).

## Phases

### Phase 1 — local drop-in/drop-out, unified camera (first landing)

1. psxport: two controller ports. A title declares `padPorts()`; with two, the second controller and the
   control channel's port-2 virtual pad drive port 2 (`p2` on `tap`/`press`/`release`/`hold`), and
   `StockPadBuffers` writes port 2's packet. A title with one port is unchanged.
2. RE the guest blocks player 2 needs (addresses in `docs/re-player-object.md` §6).
3. `x4::coop` owner (`game/coop/`): join on a pad-2 Start edge while a stage is playing and player 1 is in
   control, leave on the next one; the private context, its projection scope, player 2's pass, retried contacts,
   the midpoint camera and player 2's draw and VRAM.
4. Proof: a hidden-window run warps to a stage, taps Start on pad 2, drives Zero with pad 2, taps Start again,
   and crops show Zero joining, moving, both drawn, and the stage continuing alone; a pad replay through a build
   without the feature gives equal guest RAM while pad 2 is idle.

### Phase 2 — local split screen

Each player has their own camera record (the three layer records `0x801419B0`, stride `0x54`). The world still
updates once; what changes is admission and presentation:

- Admission: the spawn strips (`0x80028E24`/`0x80028FEC`) and the off-screen despawn verdicts run for each view,
  and an actor lives while it is inside either complete view (union of two rectangles, never the bounding box
  between them). MegaManX4Recomp's Split keeps visit history so a killed actor does not respawn at once while a
  view stays still; this port does the same.
- Presentation: after the frame's world draw, the draw section (`0x80023D68`) is re-run once per player with that
  player's camera records projected, inside psxport's draw sandbox (the one `GuestRedraw` used by
  replay-interpolation, which rolls back every write), and each pass is presented into its own pane. Menus, movies
  and shared scripted scenes show the one canonical view in both panes.
- Layout: side by side, each pane a 4:3 view, so a 16:9 window gets a letterboxed 8:3 pair (MegaManX4Recomp's
  "Side by side 8:3"); a wide pane per player is a later widescreen step because the margin columns depend on
  the widescreen background owner.
- The camera mode is `PSXPORT_X4_COOP_CAMERA` (`unified`/`split`), a saved player setting read through
  `psx::config::enh`.

### Phase 3 — LAN play

- **Owner: psxport input netplay** (`external/psxport/docs/plans/online-netplay.md`). Pads are exchanged per guest
  frame over UDP on any IP path (LAN broadcast discovery, a typed address, ZeroTier), with rollback through
  whole-machine savestates or a lockstep fallback, and no hosted service; one peer hosts. Mega Man X4 builds no
  transport, discovery, session handshake or desync detection of its own.
- **What X4 adds on top.** Two pad ports (phase 1) are the whole netplay seam: each peer supplies its own port and
  psxport supplies the other. Co-op's join, leave, projection and unified camera are guest-state logic driven only
  by the two pads, so they are deterministic on every peer by construction; no co-op state may read the host
  (which peer is local, wall clock, window). The camera mode is part of the settings the host sends.
- **Cameras.** `unified` is phase 1's midpoint camera on both peers. `everyone free` is phase 2's per-player
  cameras with each peer presenting only its own player's view full-window (MegaManX4Recomp's netplay "Split",
  where each peer sees its own seat). This is presentation and stays title-owned: admission is still the union of
  both views, computed identically on both peers, so the simulation never depends on which peer is local; only the
  pane each peer presents differs.

## Follow-ups after phase 1 (not in the first landing)

- Player 2's life gauge (MegaManX4Recomp stacks it 104 px under player 1's) and character portrait.
- Death: player 2 at zero HP is removed today; the retail death sequence, shared retries and a team wipe.
- Stage exits, checkpoints, section transitions and respawn (`engine_obj_checkpoint` `0x801721DD`).
- Bosses, boss doors and their camera locks; cutscenes and scripted scenes (one event owner, the partner held).
- Ride Armor and Ride Chasers; pause menus per player; sub-tanks and pickups ownership; weapon-get.
- Player 2's sound bank (the counterpart's voice and saber sounds live in its archive's VAB).
- Player 2 in a Zero campaign (X as player 2) is designed the same way; the first landing is proven in X's.
- Pad 2 in `.pad` recording and replay (port 2 is live-only in phase 1).

## Progress

- 2026-11-11: prior art read and recorded; psxport two-port input written; plan written. LAN transport moved
  to psxport's input netplay owner (coordinator, 2026-11-11).
- 2026-11-11: phase 1 first landing in the intro stage: join/leave on pad 2 Start, Zero walks, jumps, attacks and
  takes hits with his own tiles and stage palette, camera follows the midpoint; nobody-joined parity holds.
