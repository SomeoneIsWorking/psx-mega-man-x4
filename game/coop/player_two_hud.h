// player_two_hud.h — player 2's life gauge: the retail HUD pieces' packets, moved under player 1's and textured
// from the counterpart's UI texture copied into free VRAM tiles.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

class Core;

namespace x4::coop {

class Counterpart;

// The HUD pass 0x80024E70's pieces for the player gauge.
inline constexpr std::uint32_t kLifeBar = 0x800253F0u;      // (body, 0 player / 1 boss): the flat life bar
inline constexpr std::uint32_t kHudSprite = 0x80025188u;    // (group, frame): an assembly frame of SPRT_16 tiles
inline constexpr std::uint32_t kXWeaponGauge = 0x80024F5Cu; // (body): X's weapon energy
inline constexpr std::uint32_t kZeroGauge = 0x8002509Cu;    // (body): Zero's
inline constexpr std::uint32_t kEngineHudShown = 0x1Fu;
inline constexpr std::uint32_t kGaugeGroup = 0u;
inline constexpr std::uint32_t kLivesGroup = 1u;
inline constexpr std::uint32_t kEngineLives = 0x44u;
inline constexpr std::uint32_t kFirstDigitFrame = 0x3Bu; // + lives
inline constexpr std::uint32_t kGaugeFirstFrame = 0x45u; // + (max life - 32) / 2
inline constexpr std::uint32_t kBaseMaxLife = 32u;
// MegaManX4Recomp stacks the second gauge this far under the first.
inline constexpr std::int16_t kGaugeDrop = 104;

// Arena cursors the pieces advance: sprites, their draw modes, flat quads.
struct HudArenas {
  std::uint32_t sprites;
  std::uint32_t modes;
  std::uint32_t bars;
};
HudArenas hudArenas(Core &core);

class PlayerTwoHud final {
public:
  // Moves the packets written since `before` under player 1's gauge and points their tiles at player 2's copies.
  void place(Core &core, const Counterpart &counterpart, const HudArenas &before);
  // A stage load may reuse the tile area; the next draw uploads every tile again.
  void forget() {
    tiles_.clear();
  }

private:
  // A source tile: VRAM x of its 4-bit texels and y.
  struct Tile {
    std::int32_t x;
    std::int32_t y;
    bool operator==(const Tile &) const = default;
  };
  std::size_t slot(Core &core, const Counterpart &counterpart, Tile tile);

  std::vector<Tile> tiles_;
};

} // namespace x4::coop
