#pragma once

#include "guest_widescreen_projection.h"

class Core;

namespace x4 {

// Process-lifetime policy declaring the requested aspect.
class WidescreenPolicy final : public psx::present::GuestWidescreenProjection {
public:
  psx::present::PresentationAspect presentationAspect(const Core &core) const override;
  psx::present::HorizontalMargins pictureBeyondBuffer(Core &core) const override;
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
  // The picture columns each side of the frame being drawn; its record is sealed with the same split.
  psx::present::HorizontalMargins margins(Core &core) const;
  // The frame's ordering table went to the GPU, drawn at the view the camera still holds.
  void submitFrame(Core &core);
  // The picture beyond the buffer of the frame last submitted.
  psx::present::HorizontalMargins submittedPicture() const {
    return submitted_;
  }

private:
  ProjectionLatch latch_;
  psx::present::GuestProjectionPlan plan_;
  bool projectionPublished_ = false;
  psx::present::HorizontalMargins submitted_{psx::present::kUnboundedPicture, psx::present::kUnboundedPicture};
};

} // namespace x4
