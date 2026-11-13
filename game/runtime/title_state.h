// Mega Man X4's machine-state section: the host half of the retail task scheduler and the wide view.
#pragma once

#include "machine_state.h"

namespace x4 {

namespace bios_threads {
class Service;
}
class WidescreenController;

class TitleState final : public psx::state::NativeStatePort {
public:
  TitleState(bios_threads::Service &threads, WidescreenController &widescreen)
      : threads_(threads), widescreen_(widescreen) {}

  // Renaming the section is a format change.
  [[nodiscard]] const char *sectionName() const override {
    return "x4.tasks";
  }
  [[nodiscard]] std::uint32_t version() const override {
    return 2;
  }
  bool save(psx::platform::BlobWriter &out, std::string &error) const override;
  bool load(psx::platform::BlobReader &in, std::string &error) override;
  void restored(Core &core) override;

private:
  bios_threads::Service &threads_;
  WidescreenController &widescreen_;
};

} // namespace x4
