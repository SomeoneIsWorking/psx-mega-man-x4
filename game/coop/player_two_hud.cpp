#include "player_two_hud.h"

#include "coop.h"
#include "counterpart.h"

#include "arena_packets.h"
#include "core.h"
#include "gpu_device.h"

#include <algorithm>
#include <cstdlib>
#include <lucent/log.h>

namespace x4::coop {
namespace {

constexpr std::uint32_t kSpriteCursor = 0x1F800108u;
constexpr std::uint32_t kModeCursor = 0x1F80010Cu;
constexpr std::uint32_t kBarCursor = 0x1F800110u;
constexpr std::uint32_t kSpriteBytes = 16u; // SPRT_16: tag, color, xy, uv + CLUT
constexpr std::uint32_t kModeBytes = 8u;    // tag, 0xE1 draw mode with the texture page
constexpr std::uint32_t kBarBytes = 24u;    // POLY_F4: tag, color, four xy
constexpr std::uint32_t kTpageMask = 0x1Fu;
// GP0 words after the tag: SPRT_16 colour, xy, uv + CLUT; the draw mode's E1; POLY_F4 colour then four xy.
constexpr std::size_t kSpriteXy = 1u;
constexpr std::size_t kSpriteUvClut = 2u;
constexpr std::size_t kModeWord = 0u;
constexpr std::size_t kBarFirstCorner = 1u;
constexpr std::int32_t kPageWidth = 64; // VRAM halfwords per 4-bit texture page
constexpr std::int32_t kPageHeight = 256;
constexpr std::int32_t kTileTexels = 16;
constexpr std::int32_t kTileWidth = 4; // halfwords of a 4-bit 16-texel row
// Player 2's tiles: texture page 5 below both players' body rows, 16 across and 7 down.
constexpr std::uint32_t kSlotPage = 5u;
constexpr std::int32_t kSlotTop = 64;
constexpr std::size_t kSlotsAcross = 16u;
constexpr std::size_t kSlotCount = kSlotsAcross * 7u;

// An xy word kGaugeDrop rows lower.
std::uint32_t dropped(std::uint32_t xy) {
  const auto y = static_cast<std::uint16_t>((xy >> 16) + static_cast<std::uint32_t>(kGaugeDrop));
  return (xy & 0xFFFFu) | (static_cast<std::uint32_t>(y) << 16);
}

} // namespace

HudArenas hudArenas(Core &core) {
  return {.sprites = core.mem_r32(kSpriteCursor), .modes = core.mem_r32(kModeCursor), .bars = core.mem_r32(kBarCursor)};
}

std::size_t PlayerTwoHud::slot(Core &core, const Counterpart &counterpart, Tile tile) {
  const auto found = std::find(tiles_.begin(), tiles_.end(), tile);
  if (found != tiles_.end()) {
    return static_cast<std::size_t>(found - tiles_.begin());
  }
  if (tiles_.size() == kSlotCount) {
    lucent::error("x4-coop", "player 2's HUD needs more than {} tiles", kSlotCount);
    std::abort();
  }
  const std::size_t index = tiles_.size();
  tiles_.push_back(tile);
  std::vector<std::uint16_t> pixels;
  pixels.reserve(static_cast<std::size_t>(kTileWidth) * static_cast<std::size_t>(kTileTexels));
  for (std::int32_t row = 0; row < kTileTexels; ++row) {
    for (std::int32_t column = 0; column < kTileWidth; ++column) {
      pixels.push_back(counterpart.uiPixel(tile.x + column, tile.y + row).value_or(0u));
    }
  }
  const auto across = static_cast<std::int32_t>(index % kSlotsAcross);
  const auto down = static_cast<std::int32_t>(index / kSlotsAcross);
  core.gpuDevice().loadImage({.x = static_cast<std::int32_t>(kSlotPage) * kPageWidth + across * kTileWidth,
                              .y = kSlotTop + down * kTileTexels,
                              .width = kTileWidth,
                              .height = kTileTexels},
                             pixels,
                             core.owners.port(psx::present::WriterKind::LoadImage));
  return index;
}

void PlayerTwoHud::place(Core &core, const Counterpart &counterpart, const HudArenas &before) {
  const HudArenas after = hudArenas(core);
  if ((after.sprites - before.sprites) / kSpriteBytes != (after.modes - before.modes) / kModeBytes) {
    lucent::error("x4-coop", "player 2's HUD sprites and draw modes differ in count");
    std::abort();
  }
  std::uint32_t modeAddress = before.modes;
  for (std::uint32_t spriteAddress = before.sprites; spriteAddress < after.sprites;
       spriteAddress += kSpriteBytes, modeAddress += kModeBytes) {
    const arena::Packet sprite{spriteAddress};
    std::vector<std::uint32_t> spriteWords = arena::words(core, sprite);
    spriteWords[kSpriteXy] = dropped(spriteWords[kSpriteXy]);
    const std::uint32_t uv = spriteWords[kSpriteUvClut] & 0xFFFFu;
    const auto clut = static_cast<std::uint16_t>(spriteWords[kSpriteUvClut] >> 16);
    spriteWords[kSpriteUvClut] = uv | (static_cast<std::uint32_t>(playerTwoClut(clut)) << 16);
    const arena::Packet mode{modeAddress};
    std::vector<std::uint32_t> modeWords = arena::words(core, mode);
    const std::uint32_t page = modeWords[kModeWord] & kTpageMask;
    const Tile tile{.x = static_cast<std::int32_t>(page & 0xFu) * kPageWidth +
                         static_cast<std::int32_t>(uv & 0xFFu) / (kTileTexels / kTileWidth),
                    .y = static_cast<std::int32_t>(page >> 4) * kPageHeight + static_cast<std::int32_t>(uv >> 8)};
    if (counterpart.uiPixel(tile.x, tile.y)) {
      const std::size_t index = slot(core, counterpart, tile);
      const auto u = static_cast<std::uint32_t>((index % kSlotsAcross) * kTileTexels);
      const auto v =
          static_cast<std::uint32_t>(kSlotTop + static_cast<std::int32_t>(index / kSlotsAcross) * kTileTexels);
      spriteWords[kSpriteUvClut] = (spriteWords[kSpriteUvClut] & 0xFFFF0000u) | (v << 8) | u;
      modeWords[kModeWord] = (modeWords[kModeWord] & ~kTpageMask) | kSlotPage;
      arena::replace(core, mode, modeWords);
    }
    arena::replace(core, sprite, spriteWords);
  }
  for (std::uint32_t barAddress = before.bars; barAddress < after.bars; barAddress += kBarBytes) {
    const arena::Packet bar{barAddress};
    std::vector<std::uint32_t> barWords = arena::words(core, bar);
    for (std::size_t corner = kBarFirstCorner; corner < barWords.size(); ++corner) {
      barWords[corner] = dropped(barWords[corner]);
    }
    arena::replace(core, bar, barWords);
  }
}

} // namespace x4::coop
