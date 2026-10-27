#include "runtime/runtime.h"

#include "background_overrides.h"
#include "bios_threads.h"
#include "crt0_boot.h"
#include "cull_overrides.h"
#include "display_init.h"
#include "enhancements.h"
#include "frame/frame_driver.h"
#include "gpu_timeout.h"
#include "guest_execution.h"
#include "hud_overrides.h"
#include "movie_cleanup.h"
#include "music_stream.h"
#include "native_overrides.h"
#include "pad_layout.h"
#include "runtime/session.h"
#include "sequence_skip_overrides.h"
#include "startup_cd.h"
#include "stream_interrupt.h"
#include "stream_startup.h"
#include "title_quad.h"
#include "vram_rect_queue.h"
#include "vsync_sync.h"

#include "core.h"
#include "game.h"

#include <lucent/log.h>
#include <memory>
#include <string>

namespace x4 {
namespace {

constexpr psx::game::Crt0Image kCrt0{.entry = 0x800DAE8Cu};

constexpr psx::game::HostIdentity kHostIdentity{
    .cardDefaultPath = "scratch/saves/megamanx4.mcr",
    .userDataName = "megamanx4",
};

void synchronizePresentation(Core &core) {
  session(core).widescreen.synchronizePresentation(core);
}

} // namespace

// The retail libcd stays guest code; its object only lends its bodies to the title's CD wrappers.
Runtime::Runtime()
    : vsync_({.entry = vsync::kVSync, .queryCounter = vsync::kVblankCounter}),
      dmaCallbacks_(stream_startup::kDmaCallbackTable), libcd_({}, {}, 0),
      pad_({.slot0Buffer = pad::kSlot0Buffer, .slot1Buffer = pad::kSlot1Buffer}) {}

void Runtime::startProgram(Core &core) {
  psx::game::Crt0Boot{kCrt0}.run(core);
  lucent::info("boot", "executing the finite prefix of gameMain 0x80012024; x4::frame::FrameDriver owns repetition");
  frame::bootPrefix(core);
}

void Runtime::registerOverrides(Game &game) {
  reportUnreadEnhancements();
  // vsync::serveVSync overrides the entry and answers VSync(<0) through this binding's counter.
  vsync_.bind(game);
  dmaCallbacks_.bind(game);
  libcd_.bind(game);
  bios_threads::install(game);
  display_init::registerOverride(game.core);
  gpu_timeout::registerOverride(game.core);
  vsync::registerOverrides(game.core);
  movie_cleanup::registerOverride(game.core);
  music_stream::registerOverride(game.core);
  startup_cd::registerOverride(game.core);
  stream_interrupt::registerOverride(game.core);
  stream_startup::registerOverride(game.core);
  title_quad::registerOverride(game.core);
  cull::registerOverrides(game.core);
  background::registerOverrides(game.core);
  hud::registerOverrides(game.core);
  sequence_skip::registerOverrides(game.core);
  native_overrides::install(game.core);
}

std::unique_ptr<psx::game::TitleSessionState> Runtime::createSession(Game &game) {
  return std::make_unique<Session>(game.core, libcd_);
}

std::unique_ptr<psx::game::FrameDriver> Runtime::createFrameDriver(Game &game) {
  Session &state = session(game.core);
  return std::make_unique<frame::FrameDriver>(
      guest::callWithoutKnownReturn,
      frame::FieldServices{.deliverField = vsync::deliverField, .synchronizePresentation = synchronizePresentation},
      state.movieCleanup,
      state.musicStream,
      state.biosThreads,
      state.stageWarp);
}

void Runtime::deliverPad(Core &core, const psx::input::PadDelivery &delivery) {
  pad_.deliver(core, delivery);
}

const psx::present::GuestWidescreenProjection *Runtime::guestWidescreenProjection() const {
  return &widescreenPolicy_;
}

void Runtime::reportRun(Core &core) const {
  vram_rect::reportCensus("run end");
  bios_threads::from(core).reportCensus("run end");
}

const psx::game::HostIdentity *Runtime::hostIdentity() const {
  return &kHostIdentity;
}

} // namespace x4
