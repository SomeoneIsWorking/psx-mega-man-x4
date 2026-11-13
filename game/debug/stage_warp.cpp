#include "stage_warp.h"

#include "core.h"

#include <lucent/log.h>

namespace x4 {

bool StageWarp::stageRunning(Core &core) {
  return core.mem_r8(kEngine + kStateOffset) == kInStage && core.mem_r8(kEngine + kSubStateOffset) == kPlaying;
}

std::uint32_t StageWarp::checkpointCount(Core &core, std::uint32_t stage, std::uint32_t part) {
  const std::uint32_t list = core.mem_r32(kRoomEntries + (stage * kPartCount + part) * 4u);
  if (list == 0) {
    return 0;
  }
  std::uint32_t end = kRoomEntries;
  for (std::uint32_t slot = 0; slot < kStageNames.size() * kPartCount; ++slot) {
    const std::uint32_t other = core.mem_r32(kRoomEntries + slot * 4u);
    if (other > list && other < end) {
      end = other;
    }
  }
  return (end - list) / 4u;
}

std::string StageWarp::arm(const Arrival &arrival) {
  arrival_ = arrival;
  armed_ = true;
  return lucent::format("ok: warp armed for stage {} ({}) part {} checkpoint {}",
                        arrival_.stage,
                        kStageNames[arrival_.stage],
                        arrival_.part,
                        arrival_.checkpoint);
}

void StageWarp::applyArmed(std::uint32_t frame) {
  if (!armed_) {
    return;
  }
  armed_ = false;
  const auto [stage, part, checkpoint] = arrival_;
  if (!stageRunning(core_)) {
    lucent::error("x4-warp", "stage {} part {} dropped at f{}: no stage is running", stage, part, frame);
    return;
  }
  const bool running = core_.mem_r8(kEngine + kStageOffset) == stage && core_.mem_r8(kEngine + kPartOffset) == part;
  core_.mem_w8(kEngine + kStageOffset, static_cast<std::uint8_t>(stage));
  core_.mem_w8(kEngine + kPartOffset, static_cast<std::uint8_t>(part));
  core_.mem_w8(kEngine + kCheckpointOffset, static_cast<std::uint8_t>(checkpoint));
  core_.mem_w8(kEngine + kStageEndOffset, 0);
  core_.mem_w8(kEngine + kCheckpointMarkerOffset, 0);
  core_.mem_w8(kEngine + kSubStateOffset, 0);
  core_.mem_w8(kEngine + kStepOffset, 0);
  core_.mem_w8(kEngine + kStateOffset, running ? kSetUpStage : kLoadStage);
  lucent::info("x4-warp",
               "stage {} ({}) part {} checkpoint {} requested at f{}",
               stage,
               kStageNames[stage],
               part,
               checkpoint,
               frame);
}

} // namespace x4
