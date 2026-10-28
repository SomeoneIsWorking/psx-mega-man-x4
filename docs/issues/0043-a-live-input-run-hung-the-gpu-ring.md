# 0043 — A live-input run hung the GPU ring

Status: fixed (2026-10-28). Owner: `game/execution/bios_threads.cpp` (UpdateTasks re-entered after an over-budget task turn); the record submit path (`psxport runtime/psx/render/render_submit.cpp`)
reported it.

## Observed
`scratch/player-bugs-3/live-input/live_play.log` line 5013, 2026-10-27T23:40:24.856Z: `vkQueueSubmit
VK_ERROR_DEVICE_LOST` in `megamanx4_port`. The kernel log at the same second: `ring gfx_0.0.0 timeout ... Process
megamanx4_port pid 2700029`, then `ring reset succeeded` and `device wedged, but no recovery needed`. The run had
logged sink-idle warnings and ~800 ms stalls just before.

## Not yet known
Which submission hung the ring. The next step is to reproduce it on HEAD through the same live-input route with
`VK_LAYER_KHRONOS_validation` and the device-fault extension, and read the faulting command buffer.

## Reproduced without input, 2026-10-28
An unpaced hidden boot run with no input (ires 0, fps60 1, 3840x2160 sink, `PSXPORT_NOPACE=1`;
`scratch/player-bugs-5/paced40-margins/run.log`, a psxport build carrying an uncommitted log-only probe in the record
planner) faulted 16 s in: `[render:error] GPU fault at record: vkQueueSubmit VK_ERROR_DEVICE_LOST` at
04:20:43.227Z, kernel `ring gfx_0.0.0 timeout ... Process megamanx4_port pid 2836623` the same second, radv `This
context is innocent`. Before it the log is a continuous `sink idle, no swapchain image` / `sink resumed` pair every
field (1,000+ skipped fields), and the run's worst present interval is 1677.51 ms. The paced 40 s run on HEAD
(`paced40-final`) did not fault. So it needs no live input; the unpaced sink-idle path is the next place to look.
This fault wrote the `~/.config/psxport/gpu_fault` marker every later run of every title warns about (psxport issue
0189); the forced-fault runs of this pass rewrote it, and it was deleted on 2026-10-28.

## Cause and fix, 2026-10-28
The hung submission was a record of 32,763 copies of one full-screen semi-transparent rectangle (GP0 0x62, packet
0x80169D78, the loading fade of task `0x80012A3C`) plus one fill: 376 G fragments at 8x in one `record` submit. The
DrawOTag walk found a two-node cycle, `0x8012F498 -> 0x80169D78 -> 0x8012F498`, and stopped only at its
0x10000-node cap. The cycle was ours: when task `0x8001DAF8` ran out of turn budget, `bios_threads::Service::change`
returned to retail `UpdateTasks 0x80012600` as if the task had yielded, so the loop went on to the fade task, and
the next field `FrameDriver::stepFrame` called `UpdateTasks` again from slot 0 without the draw prefix. The fade
task ran twice into one uncleared ordering table and `AddPrim`ed its rectangle twice (store watch: the header got
next `0x12F498` at field 12419, one field after its normal add at 12418). Retail never leaves `UpdateTasks` mid-turn.

Fix: `Service::runUpdateTasks` runs `UpdateTasks` on its own fiber; an over-budget turn parks that fiber inside its
`ChangeTh` (`parkUpdateTasksWhileOverBudget`), the next field resumes the same task, and the loop continues after
it. Separately, psxport's GPU-fault latch never disabled the device, so after the loss every present piled up
memory (12 GB VRAM + 8 GB GTT in 8 s; on 2026-10-28 07:56 the kernel OOM killer took a qemu VM); the latch now
disables it, and a forced fault holds at 0.8 GB VRAM / 134 MB GTT.

Shown: five unpaced hidden boot runs of 65 s (ires 0, fps60 1, 3840x2160 sink, no input;
`scratch/player-bugs-6/ring/proof1..5`), 275-329 budget resumes each, no GPU fault, no OT cycle, no kernel ring
timeout; proof1's presents at 3000/9000/15000/21000/27000 show the intro movie, the 16:9 attract demo and later
movie frames. A review boot (`ring/review2`, 66 s) had 271 parked turns and no fault.

Limits: a task over budget under a main-side `ChangeTh` outside `UpdateTasks` has no field to park in and aborts;
no run has reached that. Ending the session while `UpdateTasks` is parked unwinds its fiber across the guest frames
on its stack, as ending one parked inside a task fiber already did.

Status: fixed.
