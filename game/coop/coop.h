// coop.h — drop-in/drop-out player 2: Start on pad 2 joins and leaves while a stage is playing
// (docs/plans/0046-drop-in-co-op.md). Player 2 plays only while player 1 is in free play; through doors,
// scripts, deaths and stage loads it is withdrawn and rejoins beside player 1. With nobody joined every override
// runs only the retail body.
#pragma once

#include "counterpart.h"
#include "player_two.h"
#include "player_two_hud.h"

#include <cstdint>
#include <string>

class Core;

namespace x4::coop {

// Retail functions the co-op owner wraps or calls.
inline constexpr std::uint32_t kGameplayTick = 0x80021158u; // engine state 6 per-field update
inline constexpr std::uint32_t kPlayerInit = 0x80035240u;   // spawns g_Player from the engine's character
inline constexpr std::uint32_t kOtReset = 0x800241E8u;      // draw pass: empties the OT buckets before the player
inline constexpr std::uint32_t kEnemyContact = 0x8002D9BCu; // an enemy body against the player; v0 1 on a hit
inline constexpr std::uint32_t kShotHit = 0x8002DD04u;      // the player's shots against an enemy; v0 the hit
inline constexpr std::uint32_t kFollowY = 0x80027A5Cu;      // camera follow, reads the player's y pixels
inline constexpr std::uint32_t kFollowX = 0x80027AACu;      // camera follow, reads the player's x pixels

// Player 2's graphics: the tile rows below player 1's at the same texture page (VRAM 0x140,0x20), and its CLUTs
// in the eight rows below the character rows 480-487 (VRAM 0,488).
inline constexpr std::int32_t kTileOriginX = 0x140;
inline constexpr std::int32_t kTileOriginY = 0x20;
inline constexpr std::uint16_t kPlayerTilePage = 0x05u;
inline constexpr std::uint16_t kTileRowsShift = 0x20u; // two 16-pixel tile rows
inline constexpr std::int32_t kCharacterClutRow = 480;
inline constexpr std::int32_t kClutRows = 8;
inline constexpr std::int32_t kClutWidth = 256;
inline constexpr std::uint16_t kClutRowsShift = kClutRows << 6; // CLUT word y is bits 6-14
inline constexpr std::int32_t kSpawnOffset = 24;                // pixels right of player 1 at a first join

// Player 2's copy of a character CLUT word: the rows below the character rows.
constexpr std::uint16_t playerTwoClut(std::uint16_t clut) {
  const std::int32_t row = (clut >> 6) & 0x1FF;
  const bool character = (clut & 0x8000u) == 0u && row >= kCharacterClutRow && row < kCharacterClutRow + kClutRows;
  return character ? static_cast<std::uint16_t>(clut + kClutRowsShift) : clut;
}

// Where player 2 is in its lifecycle.
enum class Seat : std::uint8_t {
  Absent,    // nobody on pad 2
  Playing,   // projected each field, drawn, hit and followed
  Dying,     // the retail death state runs, drawn but not hit
  Withdrawn, // held off stage until player 1 is back in free play
};

// A camera follow helper and the player body field it reads.
struct FollowAxis {
  std::uint32_t entry;
  std::uint32_t pixels;
};

class Coop final {
public:
  void tick(Core &core);
  void playerInit(Core &core);
  void drawPlayerTwo(Core &core);
  void retryHit(Core &core, std::uint32_t entry);
  void follow(Core &core, const FollowAxis &axis);
  // Player 2's gauge, from the HUD pass owner after the retail HUD.
  void drawHud(Core &core);

  // Debug: player 2's life, to reach its death; the reply answers the client.
  std::string setLife(std::uint8_t life);

  Seat seat() const {
    return seat_;
  }

private:
  class Projection;
  void join(Core &core, std::uint32_t link);
  // Places player 2 at player 1 through the retail player init; false when its assets are not on the disc.
  struct Placement {
    std::int32_t offsetX; // pixels right of player 1
    bool first;           // a join, which takes player 1's weapons
  };
  bool spawn(Core &core, std::uint32_t link, Placement placement);
  bool loadCounterpart(Core &core);
  void leave(const char *why);
  void withdraw(const char *why, bool fullLife);
  void enter(Seat seat);
  void pass(Core &core, std::uint32_t link);
  bool offView(Core &core) const;
  void uploadGraphics(Core &core);
  void drawRemapped(Core &core, std::uint32_t object, std::uint32_t link);

  PlayerTwo two_;
  Counterpart counterpart_;
  PlayerTwoHud hud_;
  std::uint32_t bodyTable_ = 0; // which of the resident body's blob tables player 2's +0x38 names
  Seat seat_ = Seat::Absent;
  std::uint32_t seatFields_ = 0; // fields since the seat changed, or player 2 last stood in view
  bool fullLifeOnReturn_ = false;
  bool joinRequested_ = false;
  bool projected_ = false;
};

void registerOverrides(Core &core);

} // namespace x4::coop
