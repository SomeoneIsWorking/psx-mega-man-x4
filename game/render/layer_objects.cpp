// SPDX-License-Identifier: AGPL-3.0-or-later
#include "layer_objects.h"

#include "arena_packets.h"
#include "camera_layers.h"
#include "core.h"
#include "native_dispatch.h"
#include "player_object.h"
#include "resumable_guest_call.h"
#include "visibility_cull.h"

#include "runtime/session.h"

namespace x4::layer_objects {
namespace {

// Guest argument register $4 (a0): the object.
constexpr int kArg0 = 4;

void drawShifted(Core &core, std::uint32_t address, const char *owner, cull::ObjectLayout layout) {
  const std::uint32_t object = core.r[kArg0];
  const int layer = cull::VisibilityCull::layerOf(core, object, layout);
  const arena::Positions before = arena::positions(core);
  psx::cpu::callOriginalResumingToReturn(core, owner, address, core.r[31]);
  const int shift = camera_layers::shiftOf(session(core).widescreen.layerShifts(core), layer);
  if (shift == 0) {
    return;
  }
  arena::forEachObjectPieceSince(core, before, [&](arena::Packet packet) {
    arena::moveX(core, packet, shift);
  });
}

// The frame's piece count (frame table entry +kCurAnim * 4, u16). With none, 0x80024334 skips its piece loop yet
// still links its tail to the unwritten draw-mode node at 0x1F800104 and makes the packet before the cursor the
// group's tail, so the next draw reuses that node in a second group and the splice links it into two slots.
std::uint32_t framePieces(Core &core, std::uint32_t object) {
  return core.mem_r16(core.mem_r32(object + guest::PlayerOffsets::kFrameTable) +
                      core.mem_r8(object + guest::PlayerOffsets::kCurAnim) * 4u);
}

void spriteBuilder(Core *core) {
  if (framePieces(*core, core->r[kArg0]) == 0) {
    return;
  }
  drawShifted(*core, kSpriteBuilder, "layer_objects::spriteBuilder original", cull::ObjectLayout::BaseObject);
}

void armorPart(Core *core) {
  drawShifted(*core, kArmorPart, "layer_objects::armorPart original", cull::ObjectLayout::BaseObject);
}

void quadDraw(Core *core) {
  drawShifted(*core, kQuadDraw, "layer_objects::quadDraw original", cull::ObjectLayout::QuadObject);
}

void quadDrawFlagged(Core *core) {
  drawShifted(*core, kQuadDrawFlagged, "layer_objects::quadDrawFlagged original", cull::ObjectLayout::QuadObject);
}

} // namespace

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kSpriteBuilder, "layer_objects::spriteBuilder", spriteBuilder);
  psx::cpu::installNativeOverride(core, kArmorPart, "layer_objects::armorPart", armorPart);
  psx::cpu::installNativeOverride(core, kQuadDraw, "layer_objects::quadDraw", quadDraw);
  psx::cpu::installNativeOverride(core, kQuadDrawFlagged, "layer_objects::quadDrawFlagged", quadDrawFlagged);
}

} // namespace x4::layer_objects
