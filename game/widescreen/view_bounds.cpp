#include "view_bounds.h"

#include "core.h"
#include "debug/stage_warp.h"
#include "title/player_object.h"

#include <algorithm>

namespace x4::view_bounds {
namespace {

constexpr int kMapBlock = 256;
constexpr int kScreenWidth = 320;

} // namespace

psx::present::HorizontalMargins pictureBeyond(Core &core) {
  if (core.mem_r8(StageWarp::kEngine + StageWarp::kStateOffset) != StageWarp::kInStage) {
    return {psx::present::kUnboundedPicture, psx::present::kUnboundedPicture};
  }
  using Offsets = guest::CameraLayerOffsets;
  const std::uint32_t camera = guest::kCameraLayersAddress;
  const int x = core.mem_r16s(camera + Offsets::kScrollX);
  int leftRoom = x - core.mem_r16s(camera + Offsets::kBoundMinX);
  int rightRoom = core.mem_r16s(camera + Offsets::kBoundMaxX) - x;
  // No layer has a tile left of map column 0 or past its last 256-pixel block.
  for (std::uint32_t slot = 0; slot < guest::kCameraLayerCount; ++slot) {
    const std::uint32_t layer = camera + slot * guest::kCameraLayerStride;
    if (core.mem_r8(layer + Offsets::kEnabled) == 0) {
      continue;
    }
    const int scroll = core.mem_r16s(layer + Offsets::kScrollX);
    const int mapWidth = (core.mem_r8(layer + Offsets::kMapRightBlock) + 1) * kMapBlock;
    leftRoom = std::min(leftRoom, scroll);
    rightRoom = std::min(rightRoom, mapWidth - kScreenWidth - scroll);
  }
  return {std::max(0, leftRoom), std::max(0, rightRoom)};
}

} // namespace x4::view_bounds
