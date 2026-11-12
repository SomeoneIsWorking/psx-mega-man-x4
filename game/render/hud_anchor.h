// SPDX-License-Identifier: AGPL-3.0-or-later
// hud_anchor.h - the SLUS_005.61 HUD packets, moved to the screen edges they hug at 4:3.
// The HUD pass (0x80024E70) draws the life, weapon and boss gauges into the packet arenas; at a wide aspect the
// left half keeps its distance from the left edge and the right half from the right edge.
#pragma once

#include "arena_packets.h"
#include "guest_widescreen_projection.h"

#include <cstdint>

class Core;

namespace x4::hud {

// The HUD pass, called once per frame from the object draw pass (@ 0x80023E60).
inline constexpr std::uint32_t kHudPass = 0x80024E70u;

// Packets at x < kCentre belong to the left edge.
inline constexpr int kCentre = 160;

class HudAnchor {
public:
  explicit HudAnchor(psx::present::HorizontalMargins margins) : margins_(margins) {}

  // Moves every packet an arena received since `before` towards its own edge.
  void anchor(Core &core, const arena::Positions &before) const;

private:
  psx::present::HorizontalMargins margins_;
};

} // namespace x4::hud
