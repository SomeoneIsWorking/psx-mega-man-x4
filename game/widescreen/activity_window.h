// SPDX-License-Identifier: AGPL-3.0-or-later
// activity_window.h - where SLUS_005.61 places and retires stage objects, widened to the presented width.
// Retail scans its placement table in strips just past the 4:3 view and retires objects 64 pixels outside it;
// at a wide aspect both follow the presentation margin, so an enemy is placed before it enters the margin.
#pragma once

#include "guest_widescreen_projection.h"

#include <cstdint>

class Core;

namespace x4::activity {

// Per-tick strip scan of the camera's movement (callers 0x800211B8, 0x80022000).
inline constexpr std::uint32_t kStripScan = 0x80028E24u;
// Whole-view scan at a room load (callers 0x8001D364, 0x8001FC20).
inline constexpr std::uint32_t kViewScan = 0x80028F58u;
// Placement scan of one box: a0-a3 left, right, top, bottom; the direction (0-4) at sp+16.
inline constexpr std::uint32_t kPlaceInBox = 0x80028FECu;
// The per-stage table the view scan hands on, entry [stage * 2 + part] of 0x800F4430.
inline constexpr std::uint32_t kPlaceStageGroup = 0x800292D0u;
inline constexpr std::uint32_t kStageGroupTable = 0x800F4430u;
inline constexpr std::uint32_t kStage = 0x801721CCu;
inline constexpr std::uint32_t kStagePart = 0x801721CDu;

// Lifetime checks: true when the object is outside the box. 0x8002B160 and 0x800B8490 keep 64 pixels of slack,
// 0x8002B1E8 takes per-side slack in a1/a2; 0x800B8490 reads layer 0 whatever the object's own layer.
inline constexpr std::uint32_t kOutsideLoose = 0x8002B160u;
inline constexpr std::uint32_t kOutsideParametric = 0x8002B1E8u;
inline constexpr std::uint32_t kOutsideLooseLayer0 = 0x800B8490u;
inline constexpr int kLifetimeSlack = 64;

// Camera layer 0: the scroll this tick (+0x0A, +0x0E) and the one the last scan saw (+0x16, +0x1A).
inline constexpr std::uint32_t kCameraX = 0x801419BAu;
inline constexpr std::uint32_t kCameraY = 0x801419BEu;
inline constexpr std::uint32_t kScannedX = 0x801419C6u;
inline constexpr std::uint32_t kScannedY = 0x801419CAu;

// The retail boxes relative to the camera (0x80028E24, 0x80028F58).
struct RetailBox {
  static constexpr int kLeft = -0x30;
  static constexpr int kRight = 0x170;
  static constexpr int kTop = -0x30;
  static constexpr int kBottom = 0x120;
  // Strip entered moving right [x + 0x150, x + 0x170), left [x - 0x30, x - 0x10).
  static constexpr int kRightStripLeft = 0x150;
  static constexpr int kLeftStripRight = -0x10;
  // Strip entered moving down [y + 0x100, y + 0x120), up [y - 0x30, y - 0x10).
  static constexpr int kDownStripTop = 0x100;
  static constexpr int kUpStripBottom = -0x10;
};

// Scan direction passed to 0x80028FEC.
enum class Direction : std::uint32_t { Whole = 0, Right = 1, Left = 2, Down = 3, Up = 4 };

struct Box {
  int left;
  int right;
  int top;
  int bottom;
  Direction direction;
};

// The boxes a tick's scan places in, `margin` guest pixels wider on each side; none when the camera did not move.
struct StripBoxes {
  bool horizontal = false;
  Box columns{};
  bool vertical = false;
  Box rows{};
};
[[nodiscard]] StripBoxes
stripBoxes(int cameraX, int cameraY, int scannedX, int scannedY, psx::present::HorizontalMargins margins);
[[nodiscard]] Box viewBox(int cameraX, int cameraY, psx::present::HorizontalMargins margins);

void registerOverrides(Core &core);

} // namespace x4::activity
