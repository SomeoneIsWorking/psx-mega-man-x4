// Mega Man X4's machine-state section: the host half of the retail task scheduler.
#pragma once

#include "machine_state.h"

namespace x4 {

namespace bios_threads {
class Service;
}

class TitleState final : public psx::state::NativeStatePort {
public:
  explicit TitleState(bios_threads::Service &threads) : threads_(threads) {}

  // Renaming the section is a format change.
  [[nodiscard]] const char *sectionName() const override {
    return "x4.tasks";
  }
  [[nodiscard]] std::uint32_t version() const override {
    return 1;
  }
  bool save(psx::platform::BlobWriter &out, std::string &error) const override;
  bool load(psx::platform::BlobReader &in, std::string &error) override;
  void restored(Core &core) override;

private:
  bios_threads::Service &threads_;
};

} // namespace x4
