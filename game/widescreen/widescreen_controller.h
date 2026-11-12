#pragma once

#include "guest_widescreen_projection.h"
#include "view_bounds.h"

class Core;

namespace x4 {

// Process-lifetime policy declaring the requested aspect.
class WidescreenPolicy final : public psx::present::GuestWidescreenProjection {
public:
  psx::present::PresentationAspect presentationAspect(const Core &core) const override;
  psx::present::PictureBeyondBuffer pictureBeyondBuffer(Core &core) const override;
};

class WidescreenController {
public:
  using GuestBody = void (*)(Core *);
  using ProjectionLatch = psx::present::GuestProjectionPlan (*)(Core &, psx::present::GuestProjectionGeometry);

  WidescreenController();
  explicit WidescreenController(ProjectionLatch latch);

  // retailBody is the original guest function run through Lightrec.
  void publishProjection(Core &core, GuestBody retailBody);
  void publishDrawEnvironment(Core &core, GuestBody retailBody);
  void synchronizePresentation(Core &core);

  const psx::present::GuestProjectionPlan &plan() const {
    return plan_;
  }
  // The view and drawn columns each side of the frame being drawn; its record is sealed with the same windows.
  psx::present::RecordWindows windows(Core &core) const;
  // The camera layers moved (after FUN_800281E8): the view of the frame now being drawn.
  void stepView(Core &core);
  // Columns layer `layer`'s retail packets and objects move in the frame being drawn (camera_layers::layerShift).
  int layerShift(Core &core, int layer) const;
  // The frame's ordering table went to the GPU; its record is sealed with this frame's view.
  void submitFrame(Core &core);
  // The view and picture beyond the buffer of the frame last submitted.
  psx::present::PictureBeyondBuffer submittedPicture() const {
    return submitted_;
  }

private:
  ProjectionLatch latch_;
  psx::present::GuestProjectionPlan plan_;
  bool projectionPublished_ = false;
  view_bounds::View view_;
  psx::present::PictureBeyondBuffer submitted_;
};

} // namespace x4
