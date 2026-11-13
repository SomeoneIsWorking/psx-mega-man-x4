// SPDX-License-Identifier: AGPL-3.0-or-later
// The retail bodies run unchanged through the scoped original call; the margin is built after the draw and
// inserted after the splice.
#include "background_overrides.h"

#include "background_tiles.h"
#include "core.h"
#include "native_dispatch.h"
#include "resumable_guest_call.h"

#include "runtime/session.h"

namespace x4::background {
namespace {

// Guest argument register $4 (a0).
constexpr int kArg0 = 4;

void original(Core *core, std::uint32_t address, const char *owner) {
  psx::cpu::callOriginalResumingToReturn(*core, owner, address, core->r[31]);
}

WideBackground wideBackground(Core &core) {
  const WidescreenController &widescreen = session(core).widescreen;
  return {widescreen.windows(core).drawn, widescreen.layerShifts(core)};
}

void drawLayer(Core *core) {
  const int layer = static_cast<int>(core->r[kArg0]);
  const std::uint32_t firstRetailSprite = core->mem_r32(kSpriteCursor);
  original(core, kDrawLayer, "background::drawLayer original");
  wideBackground(*core).drawLayer(
      *core, {.layer = layer, .firstRetailSprite = firstRetailSprite}, session(*core).marginBatch);
}

void spliceLayer(Core *core) {
  const int layer = static_cast<int>(core->r[kArg0]);
  const SpliceSlots before = spliceSlots(*core, layer);
  original(core, kSpliceLayer, "background::spliceLayer original");
  spliceMargins(*core, layer, before, session(*core).marginBatch);
}

} // namespace

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kDrawLayer, "background::drawLayer", drawLayer);
  psx::cpu::installNativeOverride(core, kSpliceLayer, "background::spliceLayer", spliceLayer);
}

} // namespace x4::background
