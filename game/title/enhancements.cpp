#include "enhancements.h"

#include <lucent/log.h>

namespace x4 {

// User preferences, so they persist in the settings file.
namespace {

psx::config::Var<bool> cv_widescreen("PSXPORT_X4_WIDESCREEN", true, psx::config::kSaved);

psx::config::Var<bool> cv_coop("PSXPORT_X4_COOP", false, psx::config::kSaved);

psx::config::Var<bool> cv_fastwait("PSXPORT_X4_FASTWAIT", true, psx::config::kSaved);

psx::config::Var<bool> cv_skip("PSXPORT_X4_SKIP", true, psx::config::kSaved);
const psx::config::Registration kRegistration{cv_widescreen, cv_coop, cv_fastwait, cv_skip};

} // namespace

psx::config::Var<bool> &widescreenCvar() {
  return cv_widescreen;
}

psx::config::Var<bool> &coopCvar() {
  return cv_coop;
}

psx::config::Var<bool> &fastWaitCvar() {
  return cv_fastwait;
}

psx::config::Var<bool> &skipCvar() {
  return cv_skip;
}

// Co-op is declared for the settings file; its feature (RE-07) does not exist yet.
void reportUnreadEnhancements() {
  lucent::info("x4-enh", "{}={} is declared but no feature reads it yet (RE-07)", cv_coop.name(), cv_coop.get());
}

} // namespace x4
