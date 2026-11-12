// SPDX-License-Identifier: AGPL-3.0-or-later
#include "layer_objects.h"

#include "arena_packets.h"
#include "camera_layers.h"
#include "core.h"
#include "native_dispatch.h"
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
  arena::forEachSince(core, before, [&](arena::Packet packet) {
    arena::shiftX(core, packet, shift);
  });
}

void spriteBuilder(Core *core) {
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
