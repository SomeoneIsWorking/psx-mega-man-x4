#include "view_overrides.h"

#include "camera_layers.h"
#include "core.h"
#include "native_dispatch.h"
#include "resumable_guest_call.h"

#include "runtime/session.h"

namespace x4::view_bounds {
namespace {

// FUN_800281E8 is the last of the three layer updates in every update pass, so the frame's camera is final here.
void lastLayerUpdate(Core *core) {
  psx::cpu::callOriginalResumingToReturn(
      *core, "view_bounds::lastLayerUpdate original", camera_layers::kLastLayerUpdate, core->r[31]);
  session(*core).widescreen.stepView(*core);
}

} // namespace

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(
      core, camera_layers::kLastLayerUpdate, "view_bounds::lastLayerUpdate", lastLayerUpdate);
}

} // namespace x4::view_bounds
