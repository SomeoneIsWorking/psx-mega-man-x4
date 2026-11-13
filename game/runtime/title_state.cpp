#include "title_state.h"

#include "bios_threads.h"

namespace x4 {

bool TitleState::save(psx::platform::BlobWriter &out, std::string &error) const {
  return threads_.save(out, error);
}

bool TitleState::load(psx::platform::BlobReader &in, std::string &error) {
  return threads_.load(in, error);
}

void TitleState::restored(Core &core) {
  (void)core;
  threads_.adoptStaged();
}

} // namespace x4
