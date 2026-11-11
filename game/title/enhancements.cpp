#include "enhancements.h"

#include "enhancement_gate.h"

#include <array>

namespace x4 {

// User preferences, so they persist in the settings file.
namespace {

psx::config::Var<bool> cv_widescreen("PSXPORT_X4_WIDESCREEN", true, psx::config::kSaved);

psx::config::Var<long> cv_coop("PSXPORT_X4_COOP", 0, psx::config::kSaved);

psx::config::Var<bool> cv_fastwait("PSXPORT_X4_FASTWAIT", true, psx::config::kSaved);

psx::config::Var<bool> cv_skip("PSXPORT_X4_SKIP", true, psx::config::kSaved);
const psx::config::Registration kRegistration{cv_widescreen, cv_coop, cv_fastwait, cv_skip};

const std::array<const char *, 2> kCoopLabels{"Off", "On"};
const std::array<psx::game::TitleIntSetting, 1> kTitleIntSettings{{
    {.id = "coop",
     .label = "Co-op (Start on pad 2)",
     .unit = "",
     .var = &cv_coop,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kCoopLabels},
}};

} // namespace

psx::config::Var<bool> &widescreenCvar() {
  return cv_widescreen;
}

psx::config::Var<long> &coopCvar() {
  return cv_coop;
}

psx::config::Var<bool> &fastWaitCvar() {
  return cv_fastwait;
}

psx::config::Var<bool> &skipCvar() {
  return cv_skip;
}

bool coopOn() {
  return psx::config::enh_int(cv_coop) != 0;
}

std::span<const psx::game::TitleIntSetting> titleIntSettings() {
  return kTitleIntSettings;
}

} // namespace x4
