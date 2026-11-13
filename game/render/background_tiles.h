// SPDX-License-Identifier: AGPL-3.0-or-later
// background_tiles.h - the SLUS_005.61 background tile layers, drawn past the 4:3 window at wide aspects.
// Retail draws 21 x 16 tile sprites per layer from a 32 x 32 cell ring; the margin columns are host packets drawn
// from the block map, so guest RAM holds only what retail wrote.
#pragma once

#include "camera_layers.h"
#include "guest_widescreen_projection.h"

#include <array>
#include <cstdint>
#include <vector>

class Core;

namespace x4::background {

// One layer's tile sprites, $a0 = layer (the loops end at 0x80026C98 / 0x80026CB0).
inline constexpr std::uint32_t kDrawLayer = 0x80026AA0u;
// Splices a layer's sprite groups into its two ordering-table slots, $a0 = layer (0x80026894).
inline constexpr std::uint32_t kSpliceLayer = 0x80026894u;

struct RetailWindow {
  static constexpr int kTile = 16;
  static constexpr int kColumns = 21;
  static constexpr int kRows = 16;
};

// Scratchpad words the layer pass reads and advances.
inline constexpr std::uint32_t kFrameIndex = 0x1F800000u;
inline constexpr std::uint32_t kMapBase = 0x1F800004u;
inline constexpr std::uint32_t kTilePool = 0x1F800008u;
inline constexpr std::uint32_t kTileAttributes = 0x1F80000Cu;
inline constexpr std::uint32_t kSpriteCursor = 0x1F800108u;
// Sprites the layer pass linked this frame; retail stops at 1000 (slti 0x3E8 @ 0x80026BB0), its arena holds 1024.
inline constexpr std::uint32_t kSpriteCount = 0x1F80011Cu;

// Map geometry (lhu @ 0x80026518, lbu @ 0x80026538): bytes of one layer's block map, and blocks per map row.
inline constexpr std::uint32_t kLayerMapBytes = 0x8013BD48u;
inline constexpr std::uint32_t kMapRowBlocks = 0x80172224u;
// A sprite group is a layer's background (group = layer) or foreground (layer + 3) tiles of one texture page
// bucket. Per frame 0xC0 bytes, group 0x20, bucket 4: the chain's tail (0x8013BD50) and its first sprite, 0 while
// empty (0x8013E2F0), both reset by 0x800267D4.
inline constexpr std::uint32_t kBucketBase = 0x8013BD50u;
inline constexpr std::uint32_t kGroupHeads = 0x8013E2F0u;
inline constexpr std::uint32_t kBucketFrameStride = 0xC0u;
inline constexpr std::uint32_t kBucketGroupStride = 0x20u;
inline constexpr std::uint32_t kForegroundGroupOffset = 3u;
inline constexpr int kBuckets = 8;
// Each group's texture-page node (one E1 word), spliced in front of its sprites: per frame 0x180, group 0x40,
// bucket 8.
inline constexpr std::uint32_t kModeNodes = 0x80171EB0u;
inline constexpr std::uint32_t kModeFrameStride = 0x180u;
inline constexpr std::uint32_t kModeGroupStride = 0x40u;
inline constexpr std::uint32_t kModeBucketStride = 8u;
// The s8 ordering-table slot of a layer's background and foreground groups (camera layer +0x4A, +0x4B).
inline constexpr std::uint32_t kBackgroundDepth = 0x4Au;
inline constexpr std::uint32_t kForegroundDepth = 0x4Bu;
// Every arena sprite is a 3-word SPRT_16 with raw texture, code 0x7D; bit 1 is the semi-transparency AA0 sets.
inline constexpr std::uint32_t kSpriteCode = 0x7D000000u;
inline constexpr std::uint32_t kSpriteSemiTransparent = 0x02000000u;
inline constexpr int kSpriteWords = 3;
// An arena slot: the tag, then the sprite.
inline constexpr std::uint32_t kSpriteBytes = 16u;

// Tile cell bits: tile index, semi-transparent, foreground group.
inline constexpr std::uint16_t kCellTileMask = 0x3FFFu;
inline constexpr std::uint16_t kCellSemiTransparent = 0x4000u;
inline constexpr std::uint16_t kCellForeground = 0x8000u;

// Columns drawn on each side of the retail window.
struct MarginColumns {
  int left;
  int right;
  int total() const {
    return left + right;
  }
};

// Columns whose sprites reach [-left, 320 + right) for this scroll.
MarginColumns marginColumns(int scrollX, psx::present::HorizontalMargins margins);

// A layer's retail window pass: the sprites it linked start at `firstRetailSprite`.
struct LayerPass {
  int layer;
  std::uint32_t firstRetailSprite;
};

// One layer's margin sprites between its draw and its splice, by side (background, foreground) and bucket.
class MarginBatch {
public:
  static constexpr int kSides = 2;

  MarginBatch();
  [[nodiscard]] int layer() const {
    return layer_;
  }
  void begin(int layer);
  std::vector<std::uint32_t> &words(int side, int bucket) {
    return words_[(static_cast<std::size_t>(side) * kBuckets) + static_cast<std::size_t>(bucket)];
  }
  void end();

private:
  int layer_ = -1;
  std::array<std::vector<std::uint32_t>, static_cast<std::size_t>(kSides *kBuckets)> words_;
};

class WideBackground {
public:
  WideBackground(psx::present::HorizontalMargins margins, const camera_layers::LayerShifts &shifts)
      : margins_(margins), shifts_(shifts) {}

  // After the retail window of a layer linked its sprites: has the walk draw them moved by the layer's shift (guest
  // RAM keeps retail's x) and builds the margin
  // columns its shifted window leaves into `batch`, from the block map (a looping layer wraps as FUN_80027344
  // does; past any other map there is no tile).
  void drawLayer(Core &core, const LayerPass &pass, MarginBatch &batch) const;

private:
  psx::present::HorizontalMargins layerMargins(int layer) const;

  psx::present::HorizontalMargins margins_;
  camera_layers::LayerShifts shifts_;
};

// The two slots a layer splices into, read before the splice: what followed each.
struct SpliceSlots {
  std::array<std::uint32_t, MarginBatch::kSides> slot;
  std::array<std::uint32_t, MarginBatch::kSides> next;
};
SpliceSlots spliceSlots(Core &core, int layer);

// After 0x80026894 spliced `layer`: inserts the batch's sprites into the ordering table as host packets, each
// where retail would have linked it. A group retail left empty gets its texture-page node too, placed where the
// splice would have put the group.
void spliceMargins(Core &core, int layer, const SpliceSlots &before, MarginBatch &batch);

} // namespace x4::background
