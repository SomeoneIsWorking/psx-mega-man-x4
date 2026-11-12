// view_bounds.h - how far the wide view can reach past the 4:3 window before it leaves the gameplay camera's bounds.
#pragma once

#include "guest_widescreen_projection.h"

class Core;

namespace x4::view_bounds {

// Columns each side the gameplay camera's clamp window (layer 0, FUN_80028BF0) still covers for a widening of
// `margin`; a side that meets a bound gives the rest to the other. Outside a running stage both sides get `margin`.
psx::present::HorizontalMargins fillableMargins(Core &core, int margin);

} // namespace x4::view_bounds
