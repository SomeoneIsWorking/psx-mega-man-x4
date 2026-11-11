#pragma once

#include "config_var.h"
#include "title_settings.h"

#include <span>

namespace x4 {

// pc_enh knobs; read them through psx::config::enh so comparison runs suppress them.
psx::config::Var<bool> &widescreenCvar(); // PSXPORT_X4_WIDESCREEN
psx::config::Var<long> &coopCvar();       // PSXPORT_X4_COOP: 0 off, 1 on
psx::config::Var<bool> &fastWaitCvar();   // PSXPORT_X4_FASTWAIT
psx::config::Var<bool> &skipCvar();       // PSXPORT_X4_SKIP

// Whether co-op is on, through psx::config::enh_int.
bool coopOn();

// The settings menu's Enhancements rows.
std::span<const psx::game::TitleIntSetting> titleIntSettings();

} // namespace x4
