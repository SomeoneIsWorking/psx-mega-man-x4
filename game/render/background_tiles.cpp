// SPDX-License-Identifier: AGPL-3.0-or-later
// background_tiles.cpp - margin columns of the SLUS_005.61 background tile layers; knows nothing of the guest ABI.
#include "background_tiles.h"

#include "camera_layers.h"
#include "core.h"
#include "draw_info.h"
#include "ordering_table.h"
#include "player_object.h"

#include <algorithm>
#include <cstdlib>
#include <lucent/log.h>

namespace x4::background {
namespace {

using guest::CameraLayerOffsets;

// Retail rounds a scroll toward zero before the shift (`bgez`/`addiu 0xF` @ 0x80026B00); margins need scroll >= 0.
constexpr int kTile = RetailWindow::kTile;
constexpr int kTileShift = 4;
constexpr int kTileMask = kTile - 1;

constexpr int ceilTiles(int pixels) {
  return pixels > 0 ? (pixels + kTileMask) >> kTileShift : 0;
}

std::uint32_t layerAddress(int layer) {
  return guest::kCameraLayersAddress + static_cast<std::uint32_t>(layer) * guest::kCameraLayerStride;
}

// A layer's view onto its map, in tiles.
struct LayerView {
  int scrollX;
  int scrollY;
  int columns; // tiles across the map
  int rows;    // tiles down the map
  int firstColumn() const {
    return scrollX >> kTileShift;
  }
  int firstRow() const {
    return scrollY >> kTileShift;
  }
  bool contains(int column, int row) const {
    return column >= 0 && column < columns && row >= 0 && row < rows;
  }
};

LayerView readLayer(Core &core, int layer) {
  const std::uint32_t base = layerAddress(layer);
  const int right = core.mem_r8(base + CameraLayerOffsets::kMapRightBlock);
  const int bottom = core.mem_r8(base + CameraLayerOffsets::kMapBottomBlock);
  return {static_cast<std::int16_t>(core.mem_r16(base + CameraLayerOffsets::kScrollX)),
          static_cast<std::int16_t>(core.mem_r16(base + CameraLayerOffsets::kScrollY)),
          (right + 1) * kTile,
          (bottom + 1) * kTile};
}

// block map byte, then the u16 cell of that block's 16 x 16 tile page (@ 0x80027518, 0x80027584).
std::uint16_t mapTile(Core &core, int layer, int column, int row) {
  const std::uint32_t rowBlocks = core.mem_r8(kMapRowBlocks);
  const std::uint32_t layerBytes = core.mem_r16(kLayerMapBytes);
  const std::uint32_t block = core.mem_r8(core.mem_r32(kMapBase) + static_cast<std::uint32_t>(layer) * layerBytes +
                                          static_cast<std::uint32_t>(row >> kTileShift) * rowBlocks +
                                          static_cast<std::uint32_t>(column >> kTileShift));
  return core.mem_r16(core.mem_r32(kTilePool) + (block << 9) + static_cast<std::uint32_t>(row & kTileMask) * 32u +
                      static_cast<std::uint32_t>(column & kTileMask) * 2u);
}

// A sprite's top-left on screen.
struct ScreenPoint {
  int x;
  int y;
};

// The words AA0 writes into an arena SPRT_16 for `cell` (@ 0x80026BC0..0x80026C84), and its group and bucket.
struct MarginSprite {
  std::array<std::uint32_t, kSpriteWords> words;
  int side;
  int bucket;
};

MarginSprite marginSprite(Core &core, std::uint16_t cell, const ScreenPoint &at) {
  const std::uint32_t attribute = core.mem_r32(core.mem_r32(kTileAttributes) + (cell & kCellTileMask) * 4u);
  const std::uint32_t clut = (((attribute & 0xF000u) >> 6) + 0x7900u) | ((attribute & 0xF00u) >> 8);
  const std::uint32_t u = (attribute >> 12) & 0xF0u;
  const std::uint32_t v = (attribute >> 16) & 0xF0u;
  return {.words = {kSpriteCode | ((cell & kCellSemiTransparent) != 0 ? kSpriteSemiTransparent : 0u),
                    (static_cast<std::uint32_t>(at.y & 0xFFFF) << 16) | static_cast<std::uint32_t>(at.x & 0xFFFF),
                    (clut << 16) | (v << 8) | u},
          .side = (cell & kCellForeground) != 0 ? 1 : 0,
          .bucket = static_cast<int>(attribute >> 24)};
}

std::uint32_t groupOf(int layer, int side) {
  return static_cast<std::uint32_t>(layer) + (side != 0 ? kForegroundGroupOffset : 0u);
}

std::uint32_t bucketWord(Core &core, std::uint32_t table, int layer, int side, int bucket) {
  return table + core.mem_r32(kFrameIndex) * kBucketFrameStride + groupOf(layer, side) * kBucketGroupStride +
         static_cast<std::uint32_t>(bucket) * 4u;
}

std::uint32_t modeNode(Core &core, int layer, int side, int bucket) {
  return kModeNodes + core.mem_r32(kFrameIndex) * kModeFrameStride + groupOf(layer, side) * kModeGroupStride +
         static_cast<std::uint32_t>(bucket) * kModeBucketStride;
}

std::uint32_t nextNode(Core &core, std::uint32_t node) {
  return psx::gpu::guestAddressOf(core.mem_r32(node) & psx::gpu::kOtNextAddressMask);
}

} // namespace

MarginBatch::MarginBatch() {
  for (std::vector<std::uint32_t> &words : words_) {
    words.reserve(static_cast<std::size_t>(RetailWindow::kRows) * RetailWindow::kColumns * kSpriteWords);
  }
}

void MarginBatch::begin(int layer) {
  if (layer_ >= 0) {
    lucent::error("x4-bg", "layer {} drew its margin while layer {}'s was never spliced", layer, layer_);
    std::abort();
  }
  layer_ = layer;
}

void MarginBatch::end() {
  layer_ = -1;
  for (std::vector<std::uint32_t> &words : words_) {
    words.clear();
  }
}

MarginColumns marginColumns(int scrollX, psx::present::HorizontalMargins margins) {
  if (margins.total() <= 0 || scrollX < 0) {
    return {0, 0};
  }
  const int fraction = scrollX & kTileMask;
  // Retail's first sprite starts at -fraction and the 21st ends at 336 - fraction.
  const int retailEnd = RetailWindow::kColumns * kTile - fraction;
  return {ceilTiles(margins.left - fraction), ceilTiles(320 + margins.right - retailEnd)};
}

psx::present::HorizontalMargins WideBackground::layerMargins(int layer) const {
  const int shift = shifts_[static_cast<std::size_t>(layer)];
  return {margins_.left + shift, margins_.right - shift};
}

void WideBackground::drawLayer(Core &core, const LayerPass &pass, MarginBatch &batch) const {
  const auto [layer, firstRetailSprite] = pass;
  const int shift = shifts_[static_cast<std::size_t>(layer)];
  if (shift != 0) {
    for (std::uint32_t retail = firstRetailSprite; retail < core.mem_r32(kSpriteCursor); retail += 16u) {
      core.mem_w16(retail + 8u, static_cast<std::uint16_t>(core.mem_r16s(retail + 8u) + shift));
    }
  }
  const LayerView view = readLayer(core, layer);
  const MarginColumns columns = marginColumns(view.scrollX, layerMargins(layer));
  if (columns.total() == 0 || view.scrollY < 0) {
    return;
  }
  batch.begin(layer);
  const camera_layers::LoopBlocks loop(core, layer);
  const int first = view.firstColumn();
  const int top = view.firstRow();
  const int originX = shift - (view.scrollX & kTileMask);
  const int originY = -(view.scrollY & kTileMask);
  for (int row = 0; row < RetailWindow::kRows; ++row) {
    for (int offset = -columns.left; offset < columns.right + RetailWindow::kColumns; ++offset) {
      if (offset >= 0 && offset < RetailWindow::kColumns) {
        continue;
      }
      const int mapColumn = loop.wrap(first + offset);
      if (!view.contains(mapColumn, top + row)) {
        continue;
      }
      const std::uint16_t cell = mapTile(core, layer, mapColumn, top + row);
      if (cell == 0) {
        continue;
      }
      const MarginSprite sprite = marginSprite(core, cell, {originX + offset * kTile, originY + row * kTile});
      std::vector<std::uint32_t> &words = batch.words(sprite.side, sprite.bucket);
      words.insert(words.end(), sprite.words.begin(), sprite.words.end());
    }
  }
}

SpliceSlots spliceSlots(Core &core, int layer) {
  const std::uint32_t drawInfo = core.mem_r32(draw_info::kCurrentDrawInfo);
  SpliceSlots slots{};
  for (int side = 0; side < MarginBatch::kSides; ++side) {
    const std::int32_t depth = core.mem_r8s(layerAddress(layer) + (side != 0 ? kForegroundDepth : kBackgroundDepth));
    slots.slot[static_cast<std::size_t>(side)] =
        draw_info::orderingTableSlot(drawInfo, static_cast<std::uint32_t>(depth));
    slots.next[static_cast<std::size_t>(side)] = nextNode(core, slots.slot[static_cast<std::size_t>(side)]);
  }
  return slots;
}

void spliceMargins(Core &core, int layer, const SpliceSlots &before, MarginBatch &batch) {
  if (batch.layer() != layer) {
    return;
  }
  const std::uint32_t root = draw_info::orderingTableRoot(core.mem_r32(draw_info::kCurrentDrawInfo));
  // Per slot, in splice order: the node a group spliced now would precede, and the groups retail left empty that
  // the margin fills, latest first, all before that node.
  struct SlotRun {
    std::uint32_t slot;
    std::uint32_t successor;
    std::vector<std::vector<std::uint32_t>> emptyGroups;
  };
  std::array<SlotRun, MarginBatch::kSides> runs{};
  for (int side = 0; side < MarginBatch::kSides; ++side) {
    runs[static_cast<std::size_t>(side)] = {
        before.slot[static_cast<std::size_t>(side)], before.next[static_cast<std::size_t>(side)], {}};
  }
  const auto runOf = [&runs](int side) -> SlotRun & {
    // Both sides may share one slot; the run is the slot's.
    return runs[side != 0 && runs[1].slot == runs[0].slot ? 0u : static_cast<std::size_t>(side)];
  };
  const auto flush = [&core, root](SlotRun &run) {
    for (auto group = run.emptyGroups.rbegin(); group != run.emptyGroups.rend(); ++group) {
      core.hostPackets.insertBefore(core, root, run.successor, *group);
    }
    run.emptyGroups.clear();
  };
  struct Inserted {
    std::size_t total = 0;
    std::size_t emptyGroupSprites = 0;
    std::size_t emptyGroups = 0;
  } inserted;
  for (int bucket = 0; bucket < kBuckets; ++bucket) {
    for (int side = 0; side < MarginBatch::kSides; ++side) {
      SlotRun &run = runOf(side);
      const std::vector<std::uint32_t> &words = batch.words(side, bucket);
      inserted.total += words.size();
      const std::uint32_t node = modeNode(core, layer, side, bucket);
      if (core.mem_r32(bucketWord(core, kGroupHeads, layer, side, bucket)) != 0) {
        // Retail spliced this group in front of the slot; the margin follows its last sprite.
        const std::uint32_t tail = core.mem_r32(bucketWord(core, kBucketBase, layer, side, bucket));
        if (nextNode(core, tail) != run.successor) {
          lucent::error("x4-bg",
                        "layer {} group {} bucket {}: the splice linked its tail 0x{:08X} to 0x{:08X}, not to 0x{:08X}",
                        layer,
                        groupOf(layer, side),
                        bucket,
                        tail,
                        nextNode(core, tail),
                        run.successor);
          std::abort();
        }
        if (!words.empty()) {
          core.hostPackets.insertBefore(core, root, nextNode(core, tail), words);
        }
        flush(run);
        run.successor = node;
        continue;
      }
      if (words.empty()) {
        continue;
      }
      inserted.emptyGroupSprites += words.size() / kSpriteWords;
      ++inserted.emptyGroups;
      const std::uint32_t header = core.mem_r32(node);
      std::vector<std::uint32_t> group;
      group.reserve((header >> 24) + words.size());
      for (std::uint32_t i = 0; i < (header >> 24); ++i) {
        group.push_back(core.mem_r32(node + 4u + i * 4u));
      }
      group.insert(group.end(), words.begin(), words.end());
      run.emptyGroups.push_back(std::move(group));
    }
  }
  for (SlotRun &run : runs) {
    flush(run);
  }
  lucent::debug("x4-bg",
                "layer {} margin: {} sprites, {} in {} group(s) retail left empty; retail linked {} of its 1000",
                layer,
                inserted.total / kSpriteWords,
                inserted.emptyGroupSprites,
                inserted.emptyGroups,
                core.mem_r32(kSpriteCount));
  batch.end();
}

} // namespace x4::background
