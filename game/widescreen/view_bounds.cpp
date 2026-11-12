#include "view_bounds.h"

#include "camera_layers.h"
#include "core.h"
#include "debug/stage_warp.h"
#include "title/player_object.h"

#include <algorithm>
#include <cstdlib>

namespace x4::view_bounds {
namespace {

constexpr int kMapBlock = 256;
constexpr int kScreenWidth = 320;
// s8 +0x47: how far FUN_80028690 lets a moving bound carry the camera per frame.
constexpr std::uint32_t kBoundStep = 0x47u;

int toward(int value, int target, int step) {
  return value < target ? std::min(target, value + step) : std::max(target, value - step);
}

// The view's offset from layer 0's clamp window; `lo > hi` is a room narrower than the wide view.
int viewOffset(Core &core, const View &previous, int x, int lo, int hi) {
  const int target = std::clamp(0, lo, hi);
  if (!previous.inStage) {
    return target;
  }
  const int moved = x - previous.cameraX;
  const int step = static_cast<std::int8_t>(core.mem_r8(guest::kCameraLayersAddress + kBoundStep));
  int offset = toward(previous.offset, target, std::max(step, 0) + std::abs(moved));
  const int viewed = previous.cameraX + previous.offset;
  if ((moved > 0 && x + offset < viewed) || (moved < 0 && x + offset > viewed)) {
    offset = viewed - x;
  }
  return std::clamp(offset, lo, hi);
}

} // namespace

View nextView(Core &core, const View &previous, int margin) {
  if (margin <= 0 || core.mem_r8(StageWarp::kEngine + StageWarp::kStateOffset) != StageWarp::kInStage) {
    return {};
  }
  using Offsets = guest::CameraLayerOffsets;
  const std::uint32_t camera = guest::kCameraLayersAddress;
  const int x = core.mem_r16s(camera + Offsets::kScrollX);
  const int leftRoom = std::max(0, x - core.mem_r16s(camera + Offsets::kBoundMinX));
  const int rightRoom = std::max(0, core.mem_r16s(camera + Offsets::kBoundMaxX) - x);
  View view{.cameraX = x, .inStage = true};
  const int lo = margin - leftRoom;
  const int hi = rightRoom - margin;
  if (lo > hi) {
    view.offset = margin - psx::present::fitMargins({leftRoom, rightRoom}, margin).left;
    view.picture = {leftRoom, rightRoom};
  } else {
    view.offset = viewOffset(core, previous, x, lo, hi);
    view.picture = {margin - view.offset, margin + view.offset};
  }
  // A layer that does not loop has no tile left of map column 0 or past its last block.
  for (int layer = 0; layer < static_cast<int>(guest::kCameraLayerCount); ++layer) {
    const std::uint32_t base = camera + static_cast<std::uint32_t>(layer) * guest::kCameraLayerStride;
    if (core.mem_r8(base + Offsets::kEnabled) == 0 || camera_layers::loops(core, layer)) {
      continue;
    }
    const int shift = camera_layers::layerShift(core, layer, view.offset);
    const int scroll = core.mem_r16s(base + Offsets::kScrollX);
    const int mapWidth = (core.mem_r8(base + Offsets::kMapRightBlock) + 1) * kMapBlock;
    view.picture.left = std::min(view.picture.left, scroll - shift);
    view.picture.right = std::min(view.picture.right, mapWidth - kScreenWidth - scroll + shift);
  }
  view.picture = {std::max(0, view.picture.left), std::max(0, view.picture.right)};
  return view;
}

} // namespace x4::view_bounds
