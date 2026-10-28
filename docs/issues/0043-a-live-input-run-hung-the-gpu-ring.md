# 0043 — A live-input run hung the GPU ring

Status: open (2026-10-28). Owner: not traced; the record submit path (`psxport runtime/psx/render/render_submit.cpp`)
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
0189).
