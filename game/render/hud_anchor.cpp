// SPDX-License-Identifier: AGPL-3.0-or-later
#include "hud_anchor.h"

#include "core.h"

#include <optional>

namespace x4::hud {

void HudAnchor::anchor(Core &core, const arena::Positions &before) const {
  if (margins_.total() == 0) {
    return;
  }
  arena::forEachSince(core, before, [&](arena::Packet packet) {
    const std::optional<int> leftmost = arena::leftmostX(core, packet);
    if (leftmost) {
      arena::moveX(core, packet, *leftmost < kCentre ? -margins_.left : margins_.right);
    }
  });
}

} // namespace x4::hud
