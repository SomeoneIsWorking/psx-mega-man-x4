#include "debug_menu.h"

#include "core.h"
#include "runtime/session.h"
#include "stage_warp.h"

#include <lucent/log.h>

#include <vector>

namespace x4 {

DebugMenu::DebugMenu() {
  warp({
      .levels = {"stage", "part"},
      .places =
          [](Core &) {
            std::vector<psx::game::WarpPlace> stages;
            for (std::uint32_t stage = 0; stage < StageWarp::kStageNames.size(); ++stage) {
              psx::game::WarpPlace place{.name = StageWarp::kStageNames[stage]};
              for (std::uint32_t part = 0; part < StageWarp::kPartCount; ++part) {
                place.places.push_back({
                    .name = lucent::format("part {}", part),
                    .arrive =
                        [stage, part](Core &core) {
                          return session(core).stageWarp.arm(stage, part);
                        },
                });
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
}

} // namespace x4
