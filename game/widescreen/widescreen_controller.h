#pragma once

#include "guest_widescreen_projection.h"

class Core;

namespace x4 {

// Process-lifetime policy declaring the requested aspect.
class WidescreenPolicy final : public psx::present::GuestWidescreenProjection {
public:
  psx::present::PresentationAspect presentationAspect(const Core &core) const override;
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

private:
  ProjectionLatch latch_;
  psx::present::GuestProjectionPlan plan_;
  bool projectionPublished_ = false;
};

} // namespace x4
