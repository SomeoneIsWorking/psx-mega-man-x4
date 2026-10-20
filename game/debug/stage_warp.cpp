#include "stage_warp.h"

#include "core.h"

#include <lucent/log.h>

namespace x4 {

const char *StageWarp::areaName(int area) const {
  return area >= 0 && area < areaCount() ? kStageNames[static_cast<std::size_t>(area)] : nullptr;
}

bool StageWarp::stageRunning(Core &core) {
  return core.mem_r8(kEngine + kStateOffset) == kInStage && core.mem_r8(kEngine + kSubStateOffset) == kPlaying;
}

std::string StageWarp::arm(const psx::debug::CommandArgs &args) {
  const std::optional<std::uint32_t> stageWord = args.number(1);
  const std::optional<std::uint32_t> partWord = args.size() < 3 ? 0u : args.number(2);
  if (!stageWord || !partWord) {
    return "usage: warp <stage> [part]";
  }
  const std::uint32_t stage = *stageWord;
  const std::uint32_t part = *partWord;
  if (stage >= kStageNames.size()) {
    return lucent::format("refused: stage {} is out of range (0..{})", stage, kStageNames.size() - 1);
  }
  if (part >= kPartCount) {
    return lucent::format("refused: part {} is out of range (0..{})", part, kPartCount - 1);
  }
  if (!stageRunning(core_)) {
    return "refused: a warp is legal only while a stage is running";
  }
  stage_ = stage;
  part_ = part;
  armed_ = true;
  return lucent::format("ok: warp armed for stage {} ({}) part {}", stage_, kStageNames[stage_], part_);
}

void StageWarp::applyArmed(std::uint32_t frame) {
  if (!armed_) {
    return;
  }
  armed_ = false;
  if (!stageRunning(core_)) {
    lucent::error("x4-warp", "stage {} part {} dropped at f{}: no stage is running", stage_, part_, frame);
    return;
  }
  core_.mem_w8(kEngine + kStageOffset, static_cast<std::uint8_t>(stage_));
  core_.mem_w8(kEngine + kPartOffset, static_cast<std::uint8_t>(part_));
  core_.mem_w8(kEngine + kStageEndOffset, 0);
  core_.mem_w8(kEngine + kCheckpointMarkerOffset, 0);
  core_.mem_w8(kEngine + kSubStateOffset, 0);
  core_.mem_w8(kEngine + kStepOffset, 0);
  core_.mem_w8(kEngine + kStateOffset, kLoadStage);
  lucent::info("x4-warp", "stage {} ({}) part {} requested at f{}", stage_, kStageNames[stage_], part_, frame);
}

} // namespace x4
