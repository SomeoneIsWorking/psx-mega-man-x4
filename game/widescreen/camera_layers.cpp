// SPDX-License-Identifier: AGPL-3.0-or-later
#include "camera_layers.h"

#include "core.h"
#include "title/player_object.h"

#include <optional>

namespace x4::camera_layers {
namespace {

using guest::CameraLayerOffsets;

// +0x44: the layer's dispatch runs only while set (FUN_80027D40 / FUN_800281E8).
constexpr std::uint32_t kFollows = 0x44u;
// +0x4D: first loop block (u8); +0x4E (kMapRightBlock) is the last.
constexpr std::uint32_t kLoopFirstBlock = 0x4Du;
// Blocks per map row (lbu @ 0x80026538); FUN_8002771C's default last loop block is this - 1.
constexpr std::uint32_t kMapRowBlocks = 0x80172224u;
constexpr int kBlockShift = 4; // tiles per 256-pixel block

std::uint32_t layerAddress(int layer) {
  return guest::kCameraLayersAddress + static_cast<std::uint32_t>(layer) * guest::kCameraLayerStride;
}

// The camera term of a layer's x for its mode; the stage's +0x40 offset cancels in a difference. Modes 1-8 of both
// tables (FUN_80027EE8.., FUN_80028390..; 2 is FUN_80028070 / FUN_80028518), with retail's arithmetic shifts.
enum class ScrollMode : std::uint8_t {
  Pick = 0,
  Half = 1,
  Camera = 2,
  Quarter = 3,
  HalfAgain = 4,
  Owned = 5, // FUN_800285E0 leaves the layer where its owner put it
  ThreeQuarters = 6,
  FiveQuarters = 7,
  ThreeHalves = 8,
};

std::optional<int> cameraTerm(ScrollMode mode, int camera) {
  switch (mode) {
  case ScrollMode::Half:
  case ScrollMode::HalfAgain:
    return camera >> 1;
  case ScrollMode::Camera:
    return camera;
  case ScrollMode::Quarter:
    return camera >> 2;
  case ScrollMode::ThreeQuarters:
    return (camera >> 1) + (camera >> 2);
  case ScrollMode::FiveQuarters:
    return camera + (camera >> 2);
  case ScrollMode::ThreeHalves:
    return camera + (camera >> 1);
  case ScrollMode::Pick:
  case ScrollMode::Owned:
    return std::nullopt;
  }
  return std::nullopt;
}

int floorDiv(int value, int divisor) {
  const int quotient = value / divisor;
  return (value % divisor != 0 && (value < 0) != (divisor < 0)) ? quotient - 1 : quotient;
}

} // namespace

int layerShift(Core &core, int layer, int viewOffset) {
  if (layer <= 0 || viewOffset == 0) {
    return 0;
  }
  const std::uint32_t base = layerAddress(layer);
  const int camera = core.mem_r16s(guest::kCameraLayersAddress + CameraLayerOffsets::kScrollX);
  const auto mode = static_cast<ScrollMode>(core.mem_r8(base + CameraLayerOffsets::kMode));
  const std::optional<int> now = cameraTerm(mode, camera);
  const std::optional<int> viewed = cameraTerm(mode, static_cast<std::int16_t>(camera + viewOffset));
  if (core.mem_r8(base + kFollows) == 0 || !now || !viewed) {
    return viewOffset;
  }
  return viewOffset - (*viewed - *now);
}

LayerShifts layerShifts(Core &core, int viewOffset) {
  LayerShifts shifts{};
  for (std::size_t layer = 0; layer < shifts.size(); ++layer) {
    shifts[layer] = layerShift(core, static_cast<int>(layer), viewOffset);
  }
  return shifts;
}

LoopBlocks::LoopBlocks(Core &core, int layer)
    : first_(static_cast<int>(core.mem_r8(layerAddress(layer) + kLoopFirstBlock))),
      last_(static_cast<int>(core.mem_r8(layerAddress(layer) + CameraLayerOffsets::kMapRightBlock))),
      loops_(first_ != 0 || static_cast<std::uint32_t>(last_) + 1u != core.mem_r8(kMapRowBlocks)) {}

int LoopBlocks::wrap(int column) const {
  const int span = last_ - first_ + 1;
  const int block = floorDiv(column, 1 << kBlockShift);
  if (!loops_ || span <= 0 || (block >= first_ && block <= last_)) {
    return column;
  }
  const int wrapped = first_ + ((block - first_) % span + span) % span;
  return column + (wrapped - block) * (1 << kBlockShift);
}

} // namespace x4::camera_layers
