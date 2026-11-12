// SPDX-License-Identifier: AGPL-3.0-or-later
// camera_layers.h - the parallax layers' camera rules (SLUS_005.61 mode tables 0x800F3140 / 0x800F3164, FUN_80027344).
#pragma once

#include <cstdint>

class Core;

namespace x4::camera_layers {

// Layer 1 and 2's per-frame dispatch (FUN_80027D40 / FUN_800281E8); 0x800281E8 runs last, after layer 0.
inline constexpr std::uint32_t kLastLayerUpdate = 0x800281E8u;

// Columns layer `layer`'s retail packets move so that, with the 4:3 window `viewOffset` columns right of the guest
// camera, the layer is drawn as at camera x + viewOffset: viewOffset - (scroll(x + viewOffset) - scroll(x)).
int layerShift(Core &core, int layer, int viewOffset);

// A layer's loop blocks (+0x4D first, +0x4E last): its map repeats between them. FUN_8002771C starts every layer on
// the whole map (0 and the row width - 1), so only a stage object that set other loop blocks makes it repeat.
class LoopBlocks {
public:
  LoopBlocks(Core &core, int layer);

  bool loops() const {
    return loops_;
  }
  // The map column (tiles) `column` reads: FUN_80027344's block wrap on a looping layer, itself otherwise.
  int wrap(int column) const;

private:
  int first_;
  int last_;
  bool loops_;
};

} // namespace x4::camera_layers
