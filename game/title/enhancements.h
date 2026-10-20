#pragma once

#include "config_var.h"

namespace x4 {

// pc_enh knobs; read them through psx::config::enh so comparison runs suppress them.
psx::config::Var<bool> &widescreenCvar(); // PSXPORT_X4_WIDESCREEN
psx::config::Var<bool> &coopCvar();       // PSXPORT_X4_COOP
psx::config::Var<bool> &fastWaitCvar();   // PSXPORT_X4_FASTWAIT
psx::config::Var<bool> &skipCvar();       // PSXPORT_X4_SKIP

// States the declared knobs no feature reads yet.
void reportUnreadEnhancements();

} // namespace x4
