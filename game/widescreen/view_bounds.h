// view_bounds.h - how far the picture reaches past the 4:3 window before it leaves the gameplay camera's bounds.
#pragma once

#include "guest_widescreen_projection.h"

class Core;

namespace x4::view_bounds {

// Columns each side of the 4:3 window the gameplay camera's clamp window (layer 0, FUN_80028BF0) and the enabled
// layers' maps still cover; unbounded outside a running stage. psx::present::fitMargins makes the split.
psx::present::HorizontalMargins pictureBeyond(Core &core);

} // namespace x4::view_bounds
