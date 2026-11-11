#include "coop.h"

#include "enhancement_gate.h"
#include "enhancements.h"
#include "player_object.h"
#include "stage_warp.h"
#include "vram_rect_queue.h"

#include "core.h"
#include "executable_write_source.h"
#include "gpu_device.h"
#include "native_dispatch.h"
#include "resumable_guest_call.h"
#include "runtime/session.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <lucent/log.h>
#include <span>

namespace x4::coop {
namespace {

// Decoded pad words are active high with the packet's bytes as hi:lo; Start is bit 3 of the first byte.
constexpr std::uint16_t kStartButton = 0x0800u;
// Engine bytes the player init rewrites or player 2's pass must not keep.
constexpr std::uint32_t kEngineBytes = 0x64u;
constexpr std::uint32_t kEngineMarker = StageWarp::kCheckpointMarkerOffset;
// The player init 0x80035240 clears the camera layers' enable bytes (0x8003558C).
constexpr std::array<std::uint32_t, 3> kCameraEnables{0x801419F4u, 0x80141A48u, 0x80141A9Cu};
// A byte player 2's pass leaves as player 1 had it.
constexpr std::uint32_t kDirtyByte = 0x80166BB0u;
// The player is in control.
constexpr std::uint8_t kControlState = 1u;
// Spawn seeding as MegaManX4Recomp's enroll.
constexpr std::uint8_t kSpawnSubState = 2u;
// The player helpers the spawn calls: assets for the state (a1 0), the collision box, terrain.
constexpr std::uint32_t kPlayerAssets = 0x800350A4u;
constexpr std::uint32_t kPlayerBox = 0x80035EA4u;
constexpr std::uint32_t kPlayerTerrain = 0x8002C614u;
// Player 2's own pass, in the tick's order: pad into the body, vehicle, state, shots, terrain, glows, trails.
constexpr std::uint32_t kPadIntoBody = 0x80035EF0u;
constexpr std::uint32_t kVehicleUpdate = 0x80021C14u;
constexpr std::uint32_t kPlayerState = 0x800311ECu;
constexpr std::uint32_t kShotsUpdate = 0x80021340u;
constexpr std::uint32_t kGlowsUpdate = 0x80021D84u;
constexpr std::uint32_t kTrailsUpdate = 0x80021CC8u;
// Draw pass pieces (0x80023DB8): the sprite builder, X's armor parts, and the pools it walks.
constexpr std::uint32_t kSpriteBuilder = 0x80024334u;
constexpr std::uint32_t kArmorParts = 0x800257BCu;
constexpr std::uint32_t kSecondBody = guest::kEntityAddress;
constexpr std::array<std::uint32_t, 2> kGlows{0x8013E470u, 0x8013E4C0u};
constexpr std::uint32_t kTrailsLast = 0x80141B70u; // drawn back to front
constexpr std::uint32_t kTrailsFirst = 0x80141AB0u;
constexpr std::uint32_t kTrailStride = 0x60u;
constexpr std::uint32_t kShotsFirst = 0x801406F8u;
constexpr std::uint32_t kShotsEnd = 0x801410B8u;
constexpr std::uint32_t kShotStride = 0x9Cu;
constexpr std::uint32_t kCharacterOffset = 0x02u; // 0 X
// Pixels apart the two players may drift, a screen less a margin; outward steps past it are undone.
constexpr std::int32_t kLeash = 240;
// Body bytes the retail scripts set while they steer player 1: a script action (0x80036AE4) and a boss door.
constexpr std::uint32_t kScriptAction = 0xC0u;
constexpr std::uint32_t kDoorAction = 0xC4u;
constexpr std::uint32_t kGroundFlags = 0x89u;
constexpr std::uint8_t kOnGround = 0x08u;
// Engine bytes a stage end and a scene set.
constexpr std::uint32_t kEngineStageEnd = StageWarp::kStageEndOffset;
constexpr std::uint32_t kEngineSceneEnd = 0x10u;
// Player 1's steering sub-states: past the spawn seeding, before the special moves.
constexpr std::uint8_t kFirstSteeringSubState = 2u;
constexpr std::uint8_t kLastSteeringSubState = 0x0Fu;
// Fields the retail death state runs before player 2 is withdrawn, and a withdrawn player waits to return.
constexpr std::uint32_t kDeathFields = 120u;
constexpr std::uint32_t kReturnFields = 60u;
// Camera layer 0 (0x801419B0) scroll pixels, and how far outside the 320 x 240 view player 2 may stand.
constexpr std::uint32_t kCameraX = 0x801419BAu;
constexpr std::uint32_t kCameraY = 0x801419BEu;
constexpr std::int32_t kViewWidth = 320;
constexpr std::int32_t kViewHeight = 240;
constexpr std::int32_t kViewMargin = 64;
constexpr std::uint32_t kOffViewFields = 60u;

// A retail function called from an override; it returns to the override's own link.
struct GuestCall {
  std::uint32_t entry;
  std::uint32_t a0 = 0;
  std::uint32_t a1 = 0;
};

void call(Core &core, const GuestCall &guestCall, std::uint32_t link) {
  core.r[4] = guestCall.a0;
  core.r[5] = guestCall.a1;
  core.r[31] = link;
  psx::cpu::callGuestToReturnResuming(core, "coop", guestCall.entry, link);
}

bool pressedStart(Core &core) {
  return (core.mem_r16(guest::kPadPressedP2) & kStartButton) != 0u;
}

// Player 1 plays the stage itself: no load, script, door, death or stage end owns it.
bool freePlay(Core &core) {
  return core.mem_r8(StageWarp::kEngine + StageWarp::kStateOffset) == StageWarp::kInStage &&
         core.mem_r8(StageWarp::kEngine + StageWarp::kSubStateOffset) == StageWarp::kPlaying &&
         core.mem_r8(StageWarp::kEngine + kEngineStageEnd) == 0u &&
         core.mem_r8(StageWarp::kEngine + kEngineSceneEnd) == 0u && core.mem_r8(kPlayer) != 0u &&
         core.mem_r8(kPlayer + kState) == kControlState && core.mem_r8(kPlayer + kScriptAction) == 0u &&
         core.mem_r8(kPlayer + kDoorAction) == 0u;
}

// Free play with player 1 standing, so player 2 can be placed on the same floor.
bool canPlace(Core &core) {
  const std::uint8_t subState = core.mem_r8(kPlayer + kSubState);
  return freePlay(core) && (core.mem_r8(kPlayer + kGroundFlags) & kOnGround) != 0u &&
         subState >= kFirstSteeringSubState && subState <= kLastSteeringSubState;
}

std::array<std::uint8_t, kEngineBytes> captureEngine(Core &core) {
  std::array<std::uint8_t, kEngineBytes> bytes{};
  for (std::uint32_t offset = 0; offset < kEngineBytes; ++offset) {
    bytes[offset] = core.mem_r8(StageWarp::kEngine + offset);
  }
  return bytes;
}

void restoreEngine(Core &core, const std::array<std::uint8_t, kEngineBytes> &bytes) {
  for (std::uint32_t offset = 0; offset < kEngineBytes; ++offset) {
    core.mem_w8(StageWarp::kEngine + offset, bytes[offset]);
  }
}

// The three decoded words at a pad's held address.
struct PadWords {
  std::array<std::uint16_t, 3> words{};
};

PadWords readPad(Core &core, std::uint32_t held) {
  return {{core.mem_r16(held), core.mem_r16(held + 2u), core.mem_r16(held + 4u)}};
}

void writePad(Core &core, std::uint32_t held, const PadWords &pad) {
  for (std::uint32_t index = 0; index < pad.words.size(); ++index) {
    core.mem_w16(held + index * 2u, pad.words[index]);
  }
}

} // namespace

class Coop::Projection final {
public:
  // `lend` also lends the counterpart's archive members, for passes that read them (update, spawn, draw).
  Projection(Coop &coop, Core &core, bool lend) : coop_(coop), core_(core), lend_(lend) {
    coop_.two_.exchange(core_);
    if (lend_) {
      coop_.counterpart_.exchange(core_);
    }
    coop_.projected_ = true;
  }
  ~Projection() {
    if (lend_) {
      coop_.counterpart_.exchange(core_);
    }
    coop_.two_.exchange(core_);
    coop_.projected_ = false;
  }
  Projection(const Projection &) = delete;
  Projection &operator=(const Projection &) = delete;

private:
  Coop &coop_;
  Core &core_;
  bool lend_;
};

void Coop::tick(Core &core) {
  const std::uint32_t link = core.r[31];
  if ((seat_ != Seat::Absent || joinRequested_) && !coopOn()) {
    joinRequested_ = false;
    leave("co-op was switched off");
  } else if (coopOn() && pressedStart(core)) {
    if (seat_ != Seat::Absent) {
      leave("pad 2 pressed Start");
    } else {
      // A join waits for player 1 to stand in free play.
      joinRequested_ = !joinRequested_;
    }
  }
  if (joinRequested_ && seat_ == Seat::Absent && canPlace(core)) {
    joinRequested_ = false;
    join(core, link);
  }
  psx::cpu::callOriginalResumingToReturn(core, "coop::tick original", kGameplayTick, link);
  ++seatFields_;
  switch (seat_) {
  case Seat::Absent:
    break;
  case Seat::Playing:
    if (!freePlay(core)) {
      withdraw("player 1 left free play", false);
      break;
    }
    pass(core, link);
    if (!two_.alive()) {
      lucent::info("x4-coop", "player 2 died at x={} y={}", two_.body16(kXPixels), two_.body16(kYPixels));
      enter(Seat::Dying);
    } else if (offView(core)) {
      if (seatFields_ >= kOffViewFields) {
        withdraw("player 2 was out of view", false);
      }
    } else {
      seatFields_ = 0;
    }
    break;
  case Seat::Dying:
    pass(core, link);
    if (seatFields_ >= kDeathFields) {
      withdraw("player 2's death sequence ended", true);
    }
    break;
  case Seat::Withdrawn:
    if (seatFields_ >= kReturnFields && canPlace(core) && spawn(core, link, {.offsetX = 0, .first = false})) {
      lucent::info("x4-coop", "player 2 returned at x={} y={}", two_.body16(kXPixels), two_.body16(kYPixels));
      enter(Seat::Playing);
    }
    break;
  }
  core.r[31] = link;
}

void Coop::enter(Seat seat) {
  seat_ = seat;
  seatFields_ = 0;
}

bool Coop::loadCounterpart(Core &core) {
  const std::uint8_t character = core.mem_r8(StageWarp::kEngine + kEngineCharacter);
  if (character > 1u) {
    lucent::warn("x4-coop", "player 1 is character {}; only X and Zero have a counterpart", character);
    return false;
  }
  const Counterpart::Identity identity{.resident = character,
                                       .stage = core.mem_r8(StageWarp::kEngine + StageWarp::kStageOffset),
                                       .section = core.mem_r8(StageWarp::kEngine + StageWarp::kPartOffset)};
  if (counterpart_.identity() != identity) {
    const std::optional<std::string> refusal =
        counterpart_.load(core, identity.resident, identity.resident ^ 1u, identity.stage, identity.section);
    if (refusal) {
      lucent::warn("x4-coop", "player 2 cannot join: {}", *refusal);
      return false;
    }
  }
  // A stage load may have reused player 2's CLUT rows and HUD tiles.
  const std::vector<std::uint8_t> &palette = counterpart_.palette();
  std::vector<std::uint16_t> clut(palette.size() / 2u);
  std::memcpy(clut.data(), palette.data(), palette.size());
  core.gpuDevice().loadImage({.x = 0, .y = kCharacterClutRow + kClutRows, .width = kClutWidth, .height = kClutRows},
                             clut,
                             core.owners.port(psx::present::WriterKind::LoadImage));
  hud_.forget();
  return true;
}

void Coop::join(Core &core, std::uint32_t link) {
  two_.clear();
  fullLifeOnReturn_ = true;
  if (!spawn(core, link, {.offsetX = kSpawnOffset, .first = true})) {
    return;
  }
  enter(Seat::Playing);
  lucent::info("x4-coop",
               "player 2 joined at x={} y={} as character {}",
               two_.body16(kXPixels),
               two_.body16(kYPixels),
               core.mem_r8(StageWarp::kEngine + kEngineCharacter) ^ 1u);
}

bool Coop::spawn(Core &core, std::uint32_t link, Placement placement) {
  if (!loadCounterpart(core)) {
    return false;
  }
  const std::uint32_t x = core.mem_r32(kPlayer + kXPos);
  const std::uint32_t y = core.mem_r32(kPlayer + kYPos);
  const std::uint8_t other = core.mem_r8(StageWarp::kEngine + kEngineCharacter) ^ 1u;
  const std::array<std::uint8_t, kEngineBytes> engine = captureEngine(core);
  std::array<std::uint8_t, kCameraEnables.size()> cameraEnabled{};
  for (std::size_t index = 0; index < kCameraEnables.size(); ++index) {
    cameraEnabled[index] = core.mem_r8(kCameraEnables[index]);
  }
  const std::uint32_t spawnX = x + (static_cast<std::uint32_t>(placement.offsetX) << 16);
  {
    const Projection projection(*this, core, true);
    core.mem_w8(StageWarp::kEngine + kEngineCharacter, other);
    core.mem_w8(StageWarp::kEngine + kEngineMarker, 0u);
    if (fullLifeOnReturn_) {
      core.mem_w8(StageWarp::kEngine + kEngineLife, engine[kEngineMaxLife]);
    }
    // A first join starts with the weapons player 1's campaign holds.
    for (std::uint32_t weapon = 0; placement.first && weapon < kEngineWeaponCount; ++weapon) {
      core.mem_w8(StageWarp::kEngine + kEngineWeaponEnergy + weapon, engine[kEngineWeaponEnergy + weapon]);
    }
    call(core, {kPlayerInit}, link);
    core.mem_w32(kPlayer + kXPos, spawnX);
    core.mem_w32(kPlayer + kYPos, y);
    core.mem_w8(kPlayer + kOnScreen, 1u);
    core.mem_w8(kPlayer + kState, kControlState);
    core.mem_w8(kPlayer + kSubState, kSpawnSubState);
    call(core, {kPlayerAssets, kPlayer, 0u}, link);
    call(core, {kPlayerBox, kPlayer}, link);
    core.mem_w32(kPlayer + kPreviousXPos, spawnX);
    core.mem_w32(kPlayer + kPreviousYPos, y);
    core.mem_w8(kPlayer + kFacing, kFacingRight);
    call(core, {kPlayerTerrain, kPlayer}, link);
    core.mem_w8(kPlayer + kFacing, kFacingRight);
    // A previous index unlike the current one makes the first pass decompress.
    core.mem_w8(kPlayer + vram_rect::kPreviousAnimOffset,
                static_cast<std::uint8_t>(~core.mem_r8(kPlayer + vram_rect::kCurrentAnimOffset)));
  }
  restoreEngine(core, engine);
  for (std::size_t index = 0; index < kCameraEnables.size(); ++index) {
    core.mem_w8(kCameraEnables[index], cameraEnabled[index]);
  }
  fullLifeOnReturn_ = false;
  return true;
}

void Coop::leave(const char *why) {
  enter(Seat::Absent);
  lucent::info("x4-coop", "player 2 left: {}", why);
  two_.clear();
}

void Coop::withdraw(const char *why, bool fullLife) {
  fullLifeOnReturn_ = fullLifeOnReturn_ || fullLife;
  if (seat_ != Seat::Withdrawn) {
    lucent::info("x4-coop", "player 2 withdrawn: {}", why);
  }
  enter(Seat::Withdrawn);
}

bool Coop::offView(Core &core) const {
  const std::int32_t x = two_.body16(kXPixels) - static_cast<std::int16_t>(core.mem_r16(kCameraX));
  const std::int32_t y = two_.body16(kYPixels) - static_cast<std::int16_t>(core.mem_r16(kCameraY));
  return x < -kViewMargin || x > kViewWidth + kViewMargin || y < -kViewMargin || y > kViewHeight + kViewMargin;
}

void Coop::pass(Core &core, std::uint32_t link) {
  const std::uint8_t dirty = core.mem_r8(kDirtyByte);
  const PadWords padOne = readPad(core, guest::kPadHeldP1);
  const std::array<std::uint8_t, kEngineBytes> engine = captureEngine(core);
  const std::int16_t playerOneX = static_cast<std::int16_t>(core.mem_r16(kPlayer + kXPixels));
  const std::int32_t before = two_.body16(kXPixels);
  writePad(core, guest::kPadHeldP1, readPad(core, guest::kPadHeldP2));
  {
    const Projection projection(*this, core, true);
    const std::uint32_t startX = core.mem_r32(kPlayer + kXPos);
    call(core, {kPadIntoBody}, link);
    call(core, {kVehicleUpdate}, link);
    call(core, {kPlayerState}, link);
    call(core, {kShotsUpdate}, link);
    call(core, {kPlayerTerrain, kPlayer}, link);
    call(core, {kGlowsUpdate}, link);
    call(core, {kTrailsUpdate}, link);
    const std::int32_t after = static_cast<std::int16_t>(core.mem_r16(kPlayer + kXPixels));
    if (std::abs(after - playerOneX) > kLeash && std::abs(after - playerOneX) > std::abs(before - playerOneX)) {
      core.mem_w32(kPlayer + kXPos, startX);
    }
    uploadGraphics(core);
    core.r[31] = link;
  }
  writePad(core, guest::kPadHeldP1, padOne);
  // The engine is player 1's: player 2's own bytes were swapped out above, the rest it may not change.
  restoreEngine(core, engine);
  core.mem_w8(kDirtyByte, dirty);
  lucent::debug("x4-coop",
                "player 2 x={} y={} state={}/{} on-screen={} animation={} life={}",
                two_.body16(kXPixels),
                two_.body16(kYPixels),
                two_.body8(kState),
                two_.body8(kSubState),
                two_.body8(kOnScreen),
                two_.body8(vram_rect::kCurrentAnimOffset),
                two_.body8(kLife));
}

void Coop::uploadGraphics(Core &core) {
  const std::uint32_t index = core.mem_r8(kPlayer + vram_rect::kCurrentAnimOffset);
  if (index == core.mem_r8(kPlayer + vram_rect::kPreviousAnimOffset)) {
    return;
  }
  // The resident body stays in place; the frame is lent as a one-entry blob at its start for the decompression.
  const std::uint32_t slot = core.mem_r32(kBodyScratchWord);
  const std::uint32_t blob = core.mem_r32(kPlayer + vram_rect::kBlobOffset);
  if (blob != slot) {
    const std::optional<std::uint32_t> table = counterpart_.residentTable(blob - slot);
    if (!table) {
      lucent::error("x4-coop", "player 2's blob 0x{:08X} is no table of the body at 0x{:08X}", blob, slot);
      std::abort();
    }
    bodyTable_ = *table;
  }
  const std::optional<Counterpart::Frame> frame = counterpart_.frame({.table = bodyTable_, .index = index});
  if (!frame) {
    lucent::error("x4-coop", "player 2's animation {} is outside body table {}", index, bodyTable_);
    std::abort();
  }
  const std::uint32_t streamOffset = (index + 1u) * 4u;
  std::vector<std::uint8_t> lent(streamOffset + frame->stream.size(), 0u);
  const std::uint32_t word = (frame->word & ~vram_rect::kStreamOffsetMask) | streamOffset;
  std::memcpy(lent.data() + static_cast<std::size_t>(index) * 4u, &word, sizeof(word));
  std::memcpy(lent.data() + streamOffset, frame->stream.data(), frame->stream.size());
  std::vector<std::byte> resident(lent.size());
  core.readBytes(slot, resident);
  core.mem_write(slot, lent, psx::cpu::ExecutableWriteSource::Native);
  core.mem_w32(kPlayer + vram_rect::kBlobOffset, slot);
  const std::optional<vram_rect::Graphic> graphic =
      vram_rect::decompress(core, kPlayer, {.x = kTileOriginX, .y = kTileOriginY});
  core.mem_write(
      slot,
      std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(resident.data()), resident.size()),
      psx::cpu::ExecutableWriteSource::Native);
  if (!graphic) {
    return;
  }
  const std::uint32_t owner = core.owners.port(psx::present::WriterKind::LoadImage);
  for (const vram_rect::Entry &entry : graphic->entries) {
    const std::size_t first = entry.pixels / 2u;
    const std::size_t count = static_cast<std::size_t>(entry.rect.w) * static_cast<std::size_t>(entry.rect.h);
    if (first + count > graphic->pixels.size()) {
      lucent::error("x4-coop", "a band at +0x{:X} of {} pixels leaves the decompression buffer", entry.pixels, count);
      std::abort();
    }
    core.gpuDevice().loadImage({.x = entry.rect.x, .y = entry.rect.y, .width = entry.rect.w, .height = entry.rect.h},
                               std::span<const std::uint16_t>(graphic->pixels).subspan(first, count),
                               owner);
  }
}

void Coop::playerInit(Core &core) {
  const std::uint32_t link = core.r[31];
  if (seat_ != Seat::Absent && !projected_) {
    withdraw("player 1 was spawned again", true);
  }
  psx::cpu::callOriginalResumingToReturn(core, "coop::playerInit original", kPlayerInit, link);
}

void Coop::drawRemapped(Core &core, std::uint32_t object, std::uint32_t link) {
  const std::uint16_t tile = core.mem_r16(object + kTileWord);
  const std::uint16_t clut = core.mem_r16(object + kClutWord);
  const bool playerTiles = (tile >> 8) == kPlayerTilePage && (tile & 0xFFu) < kTileRowsShift;
  core.mem_w16(object + kTileWord, playerTiles ? static_cast<std::uint16_t>(tile + kTileRowsShift) : tile);
  core.mem_w16(object + kClutWord, playerTwoClut(clut));
  call(core, {kSpriteBuilder, object}, link);
  core.mem_w16(object + kTileWord, tile);
  core.mem_w16(object + kClutWord, clut);
}

void Coop::drawPlayerTwo(Core &core) {
  const std::uint32_t link = core.r[31];
  psx::cpu::callOriginalResumingToReturn(core, "coop::otReset original", kOtReset, link);
  if (seat_ != Seat::Playing && seat_ != Seat::Dying) {
    return;
  }
  {
    const Projection projection(*this, core, true);
    // The order of 0x80023DB8; X's armor parts and glows are X's only.
    const bool isX = core.mem_r8(kPlayer + kCharacterOffset) == 0u;
    if (core.mem_r8(kPlayer + kOnScreen) != 0u) {
      drawRemapped(core, kPlayer, link);
      if (isX) {
        call(core, {kArmorParts, kPlayer}, link);
      }
    }
    if (core.mem_r8(kSecondBody + kOnScreen) != 0u) {
      drawRemapped(core, kSecondBody, link);
    }
    if (isX) {
      for (const std::uint32_t glow : kGlows) {
        if (core.mem_r8(glow + kOnScreen) != 0u) {
          drawRemapped(core, glow, link);
        }
      }
    }
    for (std::uint32_t trail = kTrailsLast; trail >= kTrailsFirst; trail -= kTrailStride) {
      if (core.mem_r8(trail + kOnScreen) != 0u) {
        drawRemapped(core, trail, link);
      }
    }
    for (std::uint32_t shot = kShotsFirst; shot < kShotsEnd; shot += kShotStride) {
      if (core.mem_r8(shot + kOnScreen) != 0u) {
        drawRemapped(core, shot, link);
      }
    }
  }
  core.r[31] = link;
}

void Coop::retryHit(Core &core, std::uint32_t entry) {
  const std::uint32_t link = core.r[31];
  const std::uint32_t enemy = core.r[4];
  psx::cpu::callOriginalResumingToReturn(core, "coop::hit original", entry, link);
  if (seat_ != Seat::Playing || projected_ || core.r[2] != 0u || !two_.alive()) {
    return;
  }
  const Projection projection(*this, core, false);
  core.r[4] = enemy;
  core.r[31] = link;
  psx::cpu::callOriginalResumingToReturn(core, "coop::hit player 2", entry, link);
}

void Coop::follow(Core &core, const FollowAxis &axis) {
  const std::uint32_t entry = axis.entry;
  const std::uint32_t pixels = axis.pixels;
  const std::uint32_t link = core.r[31];
  if (seat_ != Seat::Playing || projected_ || !two_.alive()) {
    psx::cpu::callOriginalResumingToReturn(core, "coop::follow original", entry, link);
    return;
  }
  const std::uint16_t own = core.mem_r16(kPlayer + pixels);
  const std::int32_t midpoint = (static_cast<std::int16_t>(own) + two_.body16(pixels)) / 2;
  core.mem_w16(kPlayer + pixels, static_cast<std::uint16_t>(midpoint));
  psx::cpu::callOriginalResumingToReturn(core, "coop::follow midpoint", entry, link);
  core.mem_w16(kPlayer + pixels, own);
}

std::string Coop::setLife(std::uint8_t life) {
  if (seat_ != Seat::Playing) {
    return "player 2 is not playing";
  }
  two_.setLife(life);
  return lucent::format("player 2's life is {}", life);
}

void Coop::drawHud(Core &core) {
  if ((seat_ != Seat::Playing && seat_ != Seat::Dying) || core.mem_r8(StageWarp::kEngine + kEngineHudShown) == 0u) {
    return;
  }
  const std::uint32_t link = core.r[31];
  const HudArenas before = hudArenas(core);
  const std::uint32_t frame = kGaugeFirstFrame + (core.mem_r8(StageWarp::kEngine + kEngineMaxLife) - kBaseMaxLife) / 2u;
  {
    const Projection projection(*this, core, true);
    call(core, {kLifeBar, kPlayer, 0u}, link);
    call(core,
         {kHudSprite, kLivesGroup, (core.mem_r8(StageWarp::kEngine + kEngineLives) + kFirstDigitFrame) & 0xFFu},
         link);
    call(core, {kHudSprite, kGaugeGroup, frame}, link);
    call(core, {core.mem_r8(kPlayer + kCharacterOffset) == 0u ? kXWeaponGauge : kZeroGauge, kPlayer}, link);
  }
  hud_.place(core, counterpart_, before);
  core.r[31] = link;
}

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kGameplayTick, "coop::tick", [](Core *active) {
    session(*active).coop.tick(*active);
  });
  psx::cpu::installNativeOverride(core, kPlayerInit, "coop::playerInit", [](Core *active) {
    session(*active).coop.playerInit(*active);
  });
  psx::cpu::installNativeOverride(core, kOtReset, "coop::drawPlayerTwo", [](Core *active) {
    session(*active).coop.drawPlayerTwo(*active);
  });
  psx::cpu::installNativeOverride(core, kEnemyContact, "coop::enemyContact", [](Core *active) {
    session(*active).coop.retryHit(*active, kEnemyContact);
  });
  psx::cpu::installNativeOverride(core, kShotHit, "coop::shotHit", [](Core *active) {
    session(*active).coop.retryHit(*active, kShotHit);
  });
  psx::cpu::installNativeOverride(core, kFollowX, "coop::followX", [](Core *active) {
    session(*active).coop.follow(*active, {.entry = kFollowX, .pixels = kXPixels});
  });
  psx::cpu::installNativeOverride(core, kFollowY, "coop::followY", [](Core *active) {
    session(*active).coop.follow(*active, {.entry = kFollowY, .pixels = kYPixels});
  });
}

} // namespace x4::coop
