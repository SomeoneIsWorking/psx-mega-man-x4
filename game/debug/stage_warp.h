// stage_warp.h — a stage/part arrival of the warp catalogue.
#pragma once

#include <array>
#include <cstdint>
#include <string>

class Core;

namespace x4 {

// Enters a stage the way FUN_800200D4 does: stage, part and checkpoint written, then engine state 4, whose loader
// FUN_80013014 loads the part's archives, or for the part already running state 5 (FUN_8001FC20), the setup retail's
// respawn after a death takes without reloading them. The checkpoint picks the room entry FUN_80028BF0 takes. Armed
// between frames, applied at the next frame boundary, and legal only while a stage is running.
class StageWarp final {
public:
  struct Arrival {
    std::uint32_t stage = 0;
    std::uint32_t part = 0;
    std::uint32_t checkpoint = 0;
  };

  // engine_obj: +0 state, +1 sub-state, +2 step, +0xC stage, +0xD part, +0xF stage-end cause,
  // +0x1D checkpoint (set by the checkpoint object FUN_800B6A90 from its +2 low nibble), +0x1E checkpoint marker.
  static constexpr std::uint32_t kEngine = 0x801721C0u;
  static constexpr std::uint32_t kStateOffset = 0x00u;
  static constexpr std::uint32_t kSubStateOffset = 0x01u;
  static constexpr std::uint32_t kStepOffset = 0x02u;
  static constexpr std::uint32_t kStageOffset = 0x0Cu;
  static constexpr std::uint32_t kPartOffset = 0x0Du;
  static constexpr std::uint32_t kStageEndOffset = 0x0Fu;
  static constexpr std::uint32_t kCheckpointOffset = 0x1Du;
  static constexpr std::uint32_t kCheckpointMarkerOffset = 0x1Eu;
  static constexpr std::uint8_t kLoadStage = 4;
  static constexpr std::uint8_t kSetUpStage = 5;
  static constexpr std::uint8_t kInStage = 6;
  static constexpr std::uint8_t kPlaying = 0; // engine state 6 sub-state 0, FUN_8001FF8C
  static constexpr std::uint32_t kPartCount = 2;
  // Stage ids 0..12; names from the MegaManX4Practice stage constants (docs/prior-art.md).
  static constexpr std::array<const char *, 13> kStageNames{"Intro",
                                                            "Web Spider",
                                                            "Frost Walrus",
                                                            "Split Mushroom",
                                                            "Magma Dragoon",
                                                            "Jet Stingray",
                                                            "Cyber Peacock",
                                                            "Storm Owl",
                                                            "Slash Beast",
                                                            "Colonel",
                                                            "Space Port",
                                                            "Final Weapon 1",
                                                            "Final Weapon 2"};

  // Room entries per stage and part, `[stage * 2 + part]`, each a list of checkpoint entries (FUN_80028BF0); a part
  // the stage lacks is 0. The lists lie back to back in table order and the last ends at the table.
  static constexpr std::uint32_t kRoomEntries = 0x800F42B4u;

  explicit StageWarp(Core &core) : core_(core) {}

  // The checkpoints of a stage's part, 0 when the stage has no such part.
  static std::uint32_t checkpointCount(Core &core, std::uint32_t stage, std::uint32_t part);

  static bool stageRunning(Core &core);
  // Arms the request; the returned text answers the client. The debug menu checked the range and stageRunning().
  std::string arm(const Arrival &arrival);
  // Writes the armed request; the frame driver calls it only at a clean frame boundary.
  void applyArmed(std::uint32_t frame);

private:
  Core &core_;
  bool armed_ = false;
  Arrival arrival_;
};

} // namespace x4
