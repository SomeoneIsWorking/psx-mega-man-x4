#include "debug_menu.h"

#include "core.h"
#include "runtime/session.h"
#include "stage_warp.h"

#include <lucent/log.h>

#include <vector>

namespace x4 {

DebugMenu::DebugMenu() {
  warp({
      .levels = {"stage", "part", "checkpoint"},
      .places =
          [](Core &core) {
            std::vector<psx::game::WarpPlace> stages;
            for (std::uint32_t stage = 0; stage < StageWarp::kStageNames.size(); ++stage) {
              psx::game::WarpPlace place{.name = StageWarp::kStageNames[stage]};
              for (std::uint32_t part = 0; part < StageWarp::kPartCount; ++part) {
                psx::game::WarpPlace partPlace{.name = lucent::format("part {}", part)};
                const std::uint32_t checkpoints = StageWarp::checkpointCount(core, stage, part);
                for (std::uint32_t checkpoint = 0; checkpoint < checkpoints; ++checkpoint) {
                  const StageWarp::Arrival arrival{.stage = stage, .part = part, .checkpoint = checkpoint};
                  partPlace.places.push_back({
                      .name = lucent::format("checkpoint {}", checkpoint),
                      .arrive =
                          [arrival](Core &core) {
                            return session(core).stageWarp.arm(arrival);
                          },
                  });
                }
                if (checkpoints > 0) {
                  place.places.push_back(std::move(partPlace));
                }
              }
              stages.push_back(std::move(place));
            }
            return stages;
          },
      .refusal =
          [](Core &core) {
            return StageWarp::stageRunning(core) ? std::string() : "a warp is legal only while a stage is running";
          },
  });
  verb({
      .name = "coop-life",
      .summary = "Set player 2's life",
      .params = {psx::game::DebugParam::integer("life", 0, 0x7F)},
      .run =
          [](const psx::game::DebugCall &call) {
            call.reply = session(call.core).coop.setLife(static_cast<std::uint8_t>(call.values.u32(0)));
          },
      .refusal =
          [](Core &core) {
            return session(core).coop.seat() == coop::Seat::Playing ? std::string() : "player 2 is not playing";
          },
      .inMenu = false,
  });
}

} // namespace x4
