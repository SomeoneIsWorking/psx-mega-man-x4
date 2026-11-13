#include "title_state.h"

#include "bios_threads.h"
#include "widescreen_controller.h"

namespace x4 {

bool TitleState::save(psx::platform::BlobWriter &out, std::string &error) const {
  if (!threads_.save(out, error)) {
    return false;
  }
  widescreen_.saveView(out);
  return true;
}

bool TitleState::load(psx::platform::BlobReader &in, std::string &error) {
  return threads_.load(in, error) && widescreen_.loadView(in, error);
}

void TitleState::restored(Core &core) {
  (void)core;
  threads_.adoptStaged();
}

} // namespace x4
