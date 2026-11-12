// SPDX-License-Identifier: AGPL-3.0-or-later
// layer_objects.h - objects drawn against a parallax layer move with that layer's view shift.
// The four object draws of the draw pass (0x80023DB8) place an object at its position minus the scroll of the
// camera layer its background byte names; the packets they write move by that layer's
// WidescreenController::layerShifts entry, as the layer's own tiles do (WideBackground::drawLayer).
#pragma once

#include <cstdint>

class Core;

namespace x4::layer_objects {

// Sprite builder (BaseObj +0x14), also called by co-op for player 2.
inline constexpr std::uint32_t kSpriteBuilder = 0x80024334u;
// X's armor parts (BaseObj +0x14), from 0x800257BC.
inline constexpr std::uint32_t kArmorPart = 0x8002588Cu;
// Quad pool draws (QuadObj +0x37): flat, and with bit 1 of the first byte set.
inline constexpr std::uint32_t kQuadDraw = 0x80024920u;
inline constexpr std::uint32_t kQuadDrawFlagged = 0x80024B9Cu;

void registerOverrides(Core &core);

} // namespace x4::layer_objects
