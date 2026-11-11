// player_two.h — player 2 as a private copy of the per-player guest blocks, projected into the singleton player
// for its own passes (docs/plans/0046-drop-in-co-op.md, docs/re-player-object.md §6).
#pragma once

#include "player_object.h"
#include "stage_warp.h"

#include <array>
#include <cstdint>
#include <vector>

class Core;

namespace x4::coop {

// The engine's per-player singletons: each is swapped with player 2's copy while player 2 is projected.
struct Block {
  std::uint32_t address;
  std::uint32_t size;
};
inline constexpr std::uint32_t kPlayer = guest::kPlayerAddress;
// Engine bytes per player.
inline constexpr std::uint32_t kEngineCharacter = guest::kEngineCharacterAddress - StageWarp::kEngine;
inline constexpr std::uint32_t kEngineLife = 0x45u;
inline constexpr std::uint32_t kEngineMaxLife = 0x46u;
inline constexpr std::uint32_t kEngineWeaponEnergy = 0x49u;
inline constexpr std::uint32_t kEngineWeaponCount = 16u;
inline constexpr std::uint32_t kEngineWeapon = 0x60u;
inline constexpr std::array<Block, 10> kPlayerBlocks{{
    {kPlayer, 0xE4u},
    {guest::kEntityAddress, 0xE4u}, // the second body 0x80035EF0 feeds when body+0xDE is set
    {0x801406F8u, 0x9C0u},          // shots, 16 x 0x9C (0x80021340)
    {0x80141AB0u, 0x120u},          // dash trails, 3 x 0x60 (0x80021CC8)
    {0x8013E470u, 0xA0u},           // charge glows, 2 x 0x50 (0x80021D84)
    {0x80173A30u, 0xB0u},           // ride armor or chaser (0x80021C14)
    {0x80172203u, 1u},              // engine +0x43 character, read by the player init 0x80035240
    {0x80172205u, 1u},              // engine +0x45 life
    {0x80172209u, 16u},             // engine +0x49 weapon energy
    {0x80172220u, 1u},              // engine +0x60 weapon
}};

// Body fields player 2 reads or seeds.
inline constexpr std::uint32_t kOnScreen = 0x03u;
inline constexpr std::uint32_t kState = 0x04u;
inline constexpr std::uint32_t kSubState = 0x05u;
inline constexpr std::uint32_t kXPos = 0x08u; // s16.16
inline constexpr std::uint32_t kXPixels = 0x0Au;
inline constexpr std::uint32_t kYPos = 0x0Cu;
inline constexpr std::uint32_t kYPixels = 0x0Eu;
inline constexpr std::uint32_t kFacing = 0x15u; // 0 left, 0x40 right
inline constexpr std::uint32_t kPreviousXPos = 0x18u;
inline constexpr std::uint32_t kPreviousYPos = 0x1Cu;
inline constexpr std::uint32_t kTileWord = 0x40u; // high byte texture page, low byte 16 px tile row<<4|column
inline constexpr std::uint32_t kClutWord = 0x42u; // y<<6 | x/16; bit 15 the builder's flash CLUT
inline constexpr std::uint32_t kLife = 0x5Cu;     // low seven bits
inline constexpr std::uint8_t kFacingRight = 0x40u;
// Alive as MegaManX4Recomp reads it: active, life left, a living state.
inline constexpr std::uint8_t kFirstDeadState = 2u;

// One copy of every block, in kPlayerBlocks order.
class PlayerTwo final {
public:
  PlayerTwo();

  // Exchanges the copy with the guest's singletons; two calls restore both.
  void exchange(Core &core);
  void clear();

  std::uint8_t body8(std::uint32_t offset) const;
  std::int16_t body16(std::uint32_t offset) const;
  bool alive() const;

private:
  std::vector<std::uint8_t> bytes_;
};

} // namespace x4::coop
