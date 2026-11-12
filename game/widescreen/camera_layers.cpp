// SPDX-License-Identifier: AGPL-3.0-or-later
#include "camera_layers.h"

#include "core.h"
#include "title/player_object.h"

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
bool cameraTerm(int mode, int camera, int &term) {
  switch (mode) {
  case 1:
  case 4:
    term = camera >> 1;
    return true;
  case 2:
    term = camera;
    return true;
  case 3:
    term = camera >> 2;
    return true;
  case 6:
    term = (camera >> 1) + (camera >> 2);
    return true;
  case 7:
    term = camera + (camera >> 2);
    return true;
  case 8:
    term = camera + (camera >> 1);
    return true;
  default:
    return false; // 0 picks the mode, 5 (FUN_800285E0) leaves the layer where its owner put it
  }
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
  const int mode = core.mem_r8(base + CameraLayerOffsets::kMode);
  int now = 0;
  int viewed = 0;
  if (core.mem_r8(base + kFollows) == 0 || !cameraTerm(mode, camera, now) ||
      !cameraTerm(mode, static_cast<std::int16_t>(camera + viewOffset), viewed)) {
    return viewOffset;
  }
  return viewOffset - (viewed - now);
}

bool loops(Core &core, int layer) {
  const std::uint32_t base = layerAddress(layer);
  return core.mem_r8(base + kLoopFirstBlock) != 0 ||
         core.mem_r8(base + CameraLayerOffsets::kMapRightBlock) + 1u != core.mem_r8(kMapRowBlocks);
}

int wrappedColumn(Core &core, int layer, int column) {
  const std::uint32_t base = layerAddress(layer);
  const int first = core.mem_r8(base + kLoopFirstBlock);
  const int last = core.mem_r8(base + CameraLayerOffsets::kMapRightBlock);
  const int span = last - first + 1;
  const int block = floorDiv(column, 1 << kBlockShift);
  if (span <= 0 || (block >= first && block <= last)) {
    return column;
  }
  const int wrapped = first + ((block - first) % span + span) % span;
  return column + (wrapped - block) * (1 << kBlockShift);
}

} // namespace x4::camera_layers
