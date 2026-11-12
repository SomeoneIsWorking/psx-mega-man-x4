// view_overrides.h - the view camera stepped once per frame, after the guest's camera layers moved.
#pragma once

class Core;

namespace x4::view_bounds {

void registerOverrides(Core &core);

} // namespace x4::view_bounds
