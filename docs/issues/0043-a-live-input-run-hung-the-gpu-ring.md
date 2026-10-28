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
