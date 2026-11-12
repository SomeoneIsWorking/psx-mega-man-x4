// SPDX-License-Identifier: AGPL-3.0-or-later
// activity_window.cpp - the SLUS_005.61 placement scans and lifetime checks, widened by the presentation margin.
#include "activity_window.h"

#include "core.h"
#include "native_dispatch.h"
#include "resumable_guest_call.h"
#include "runtime/session.h"
#include "visibility_cull.h"

namespace x4::activity {
namespace {

constexpr int kArg0 = 4;
constexpr int kArg1 = 5;
constexpr int kArg2 = 6;
constexpr int kArg3 = 7;
constexpr int kReturnValue = 2;
constexpr int kStackPointer = 29;
constexpr int kReturnAddress = 31;
// The caller's frame: the four argument slots, then the fifth argument at sp+16 (`lw 0x10(sp)` in 0x80028FEC).
constexpr std::uint32_t kCallFrameBytes = 24u;
constexpr std::uint32_t kFifthArgument = 16u;

// The retail boxes are passed as s16 (`sll 16; sra 16` before each `jal 0x80028FEC`).
std::uint32_t guestShort(int value) {
  return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value)));
}

int readShort(Core &core, std::uint32_t address) {
  return static_cast<std::int16_t>(core.mem_r16(address));
}

int presentationMargin(Core &core) {
  return cull::VisibilityCull{session(core).widescreen.plan()}.horizontalMargin();
}

void placeInBox(Core &core, const Box &box, std::uint32_t link) {
  core.r[kStackPointer] -= kCallFrameBytes;
  core.mem_w32(core.r[kStackPointer] + kFifthArgument, static_cast<std::uint32_t>(box.direction));
  core.r[kArg0] = guestShort(box.left);
  core.r[kArg1] = guestShort(box.right);
  core.r[kArg2] = guestShort(box.top);
  core.r[kArg3] = guestShort(box.bottom);
  core.r[kReturnAddress] = link;
  psx::cpu::callGuestToReturnResuming(core, "activity::placeInBox", kPlaceInBox, link);
  core.r[kStackPointer] += kCallFrameBytes;
}

void stripScan(Core *core) {
  const std::uint32_t link = core->r[kReturnAddress];
  const StripBoxes boxes = stripBoxes(readShort(*core, kCameraX),
                                      readShort(*core, kCameraY),
                                      readShort(*core, kScannedX),
                                      readShort(*core, kScannedY),
                                      presentationMargin(*core));
  if (boxes.horizontal) {
    placeInBox(*core, boxes.columns, link);
  }
  if (boxes.vertical) {
    placeInBox(*core, boxes.rows, link);
  }
  core->r[kReturnAddress] = link;
}

void viewScan(Core *core) {
  const std::uint32_t link = core->r[kReturnAddress];
  placeInBox(*core, viewBox(readShort(*core, kCameraX), readShort(*core, kCameraY), presentationMargin(*core)), link);
  const std::uint32_t entry =
      kStageGroupTable +
      (core->mem_r8(kStage) * 2u + static_cast<std::uint32_t>(static_cast<std::int8_t>(core->mem_r8(kStagePart)))) * 4u;
  core->r[kArg0] = core->mem_r32(entry);
  core->r[kReturnAddress] = link;
  psx::cpu::callGuestToReturnResuming(*core, "activity::placeStageGroup", kPlaceStageGroup, link);
  core->r[kReturnAddress] = link;
}

void publishOutside(Core &core, bool inside) {
  core.r[kReturnValue] = inside ? 0u : 1u;
}

bool insideLifetime(Core &core, std::uint32_t object, int halfWidth, int halfHeight) {
  const cull::VisibilityCull window{session(core).widescreen.plan()};
  const cull::ScreenCoordinate x = window.screenX(core, object, cull::ObjectLayout::BaseObject);
  const cull::ScreenCoordinate y = window.screenY(core, object, cull::ObjectLayout::BaseObject);
  return window.inside(x.value, y.value, halfWidth, halfHeight);
}

void outsideLoose(Core *core) {
  publishOutside(*core, insideLifetime(*core, core->r[kArg0], kLifetimeSlack, kLifetimeSlack));
}

// a1/a2 are s16 half-extents (`sll 16; sra 16` at entry).
void outsideParametric(Core *core) {
  publishOutside(
      *core,
      insideLifetime(
          *core, core->r[kArg0], static_cast<std::int16_t>(core->r[kArg1]), static_cast<std::int16_t>(core->r[kArg2])));
}

void outsideLooseLayer0(Core *core) {
  const std::uint32_t object = core->r[kArg0];
  const int x = static_cast<std::int16_t>(core->mem_r16(object + cull::kPositionIntegerX) - core->mem_r16(kCameraX));
  const int y = static_cast<std::int16_t>(core->mem_r16(object + cull::kPositionIntegerY) - core->mem_r16(kCameraY));
  const cull::VisibilityCull window{session(*core).widescreen.plan()};
  publishOutside(*core, window.inside(x, y, kLifetimeSlack, kLifetimeSlack));
}

} // namespace

StripBoxes stripBoxes(int cameraX, int cameraY, int scannedX, int scannedY, int margin) {
  StripBoxes boxes;
  if (scannedX != cameraX) {
    const bool right = scannedX < cameraX;
    const int left = right ? cameraX + RetailBox::kRightStripLeft + margin : cameraX + RetailBox::kLeft - margin;
    const int rightEdge = right ? cameraX + RetailBox::kRight + margin : cameraX + RetailBox::kLeftStripRight - margin;
    boxes.horizontal = true;
    boxes.columns = {left,
                     rightEdge,
                     cameraY + RetailBox::kTop,
                     cameraY + RetailBox::kBottom,
                     right ? Direction::Right : Direction::Left};
  }
  if (scannedY != cameraY) {
    const bool down = scannedY < cameraY;
    boxes.vertical = true;
    boxes.rows = {cameraX + RetailBox::kLeft - margin,
                  cameraX + RetailBox::kRight + margin,
                  down ? cameraY + RetailBox::kDownStripTop : cameraY + RetailBox::kTop,
                  down ? cameraY + RetailBox::kBottom : cameraY + RetailBox::kUpStripBottom,
                  down ? Direction::Down : Direction::Up};
  }
  return boxes;
}

Box viewBox(int cameraX, int cameraY, int margin) {
  return {cameraX + RetailBox::kLeft - margin,
          cameraX + RetailBox::kRight + margin,
          cameraY + RetailBox::kTop,
          cameraY + RetailBox::kBottom,
          Direction::Whole};
}

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kStripScan, "activity::stripScan", stripScan);
  psx::cpu::installNativeOverride(core, kViewScan, "activity::viewScan", viewScan);
  psx::cpu::installNativeOverride(core, kOutsideLoose, "activity::outsideLoose", outsideLoose);
  psx::cpu::installNativeOverride(core, kOutsideParametric, "activity::outsideParametric", outsideParametric);
  psx::cpu::installNativeOverride(core, kOutsideLooseLayer0, "activity::outsideLooseLayer0", outsideLooseLayer0);
}

} // namespace x4::activity
