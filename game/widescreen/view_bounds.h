// view_bounds.h - the host view camera: where the wide view stands against the gameplay camera's bounds.
#pragma once

#include "guest_widescreen_projection.h"

class Core;

namespace x4::view_bounds {

// The view for one frame: the 4:3 window sits `offset` columns right of the guest camera inside the wide view, which
// holds `picture` beyond the window each side (psx::present::fitMargins makes the split).
struct View {
  int offset = 0;
  psx::present::HorizontalMargins picture{psx::present::kUnboundedPicture, psx::present::kUnboundedPicture};
  int cameraX = 0;
  bool inStage = false;
};

// The view after the camera layers moved this frame. The split comes from layer 0's clamp window (FUN_80028BF0,
// moved by FUN_80028690) alone; a view leaving a bound that relaxed returns to the camera at most +0x47 columns
// per frame beyond the camera's own motion, and never against it. A layer whose map ends there narrows the picture.
View nextView(Core &core, const View &previous, int margin);

} // namespace x4::view_bounds
