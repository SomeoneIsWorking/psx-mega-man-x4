#include "debug_menu.h"

#include "core.h"
#include "runtime/session.h"
#include "stage_warp.h"

namespace x4 {

DebugMenu::DebugMenu() {
  using psx::game::DebugCall;
  using psx::game::DebugNames;
  using psx::game::DebugParam;
  section("Stage Warp");
  verb({
      .name = "warp",
      .summary = "Load that stage",
      .params = {DebugParam::named("stage", DebugNames::of(StageWarp::kStageNames)),
                 DebugParam::integer("part", 0, StageWarp::kPartCount - 1).orAbsent()},
      .run =
          [](const DebugCall &call) {
            call.reply = session(call.core).stageWarp.arm(call.values.u32(0), call.values.u32(1));
          },
      .refusal =
          [](Core &core) {
            return StageWarp::stageRunning(core) ? std::string() : "a warp is legal only while a stage is running";
          },
  });
}

} // namespace x4
