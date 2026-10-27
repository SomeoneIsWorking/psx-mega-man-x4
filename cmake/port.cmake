include(${PSXPORT_DIR}/cmake/psxport.cmake)

# Include path shared by every first-party target.
set(X4_TITLE_DIRS
  ${CMAKE_SOURCE_DIR}/game
  ${CMAKE_SOURCE_DIR}/game/debug
  ${CMAKE_SOURCE_DIR}/game/execution
  ${CMAKE_SOURCE_DIR}/game/frame
  ${CMAKE_SOURCE_DIR}/game/input
  ${CMAKE_SOURCE_DIR}/game/media
  ${CMAKE_SOURCE_DIR}/game/render
  ${CMAKE_SOURCE_DIR}/game/title
  ${CMAKE_SOURCE_DIR}/game/ui
  ${CMAKE_SOURCE_DIR}/game/widescreen
)

set(SEAM_SRC
  game/runtime/runtime.cpp
  game/runtime/session.cpp
  game/frame/frame_driver.cpp
  game/frame/vsync_sync.cpp
  game/input/input_path.cpp
  game/media/startup_cd.cpp
  game/media/cd_controller.cpp
  game/media/cd_control_boundary.cpp
  game/media/stream_startup.cpp
  game/media/stream_interrupt.cpp
  game/media/movie_cleanup.cpp
  game/media/music_cd.cpp
  game/media/music_stream.cpp
  game/media/fast_wait.cpp
  game/render/display_init.cpp
  game/render/gpu_timeout.cpp
  game/render/background_tiles.cpp
  game/render/background_overrides.cpp
  game/render/hud_anchor.cpp
  game/render/hud_overrides.cpp
  game/input/sequence_skip.cpp
  game/input/sequence_skip_overrides.cpp
  game/render/visibility_cull.cpp
  game/render/widened_objects.cpp
  game/render/cull_overrides.cpp
  game/render/vram_rect_queue.cpp
  game/widescreen/widescreen_controller.cpp
  game/ui/title_quad.cpp
  game/execution/guest_execution.cpp
  game/execution/native_overrides.cpp
  game/execution/bios_threads.cpp
  game/debug/debug_menu.cpp
  game/debug/stage_warp.cpp
  game/title/enhancements.cpp
)
add_library(megamanx4_seam OBJECT ${SEAM_SRC})
# C++20: enhancements.cpp uses psx::config::BoolVar, whose header needs it.
set_target_properties(megamanx4_seam PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(megamanx4_seam PRIVATE ${X4_TITLE_DIRS})
# Links only for include directories; an OBJECT library has no link step.
target_link_libraries(megamanx4_seam PRIVATE psxport)
target_compile_options(megamanx4_seam PRIVATE -g)


psxport_add_product(
  megamanx4_port
  CATALOG x4::createCatalog
  HEADER runtime/catalog.h
  SOURCES game/runtime/catalog.cpp ${SEAM_SRC}
)

# libpsxport's render sources include the generated shader header.
add_dependencies(megamanx4_port gen_gpu_shaders)

set_target_properties(megamanx4_port PROPERTIES
  CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
  ENABLE_EXPORTS ON
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

target_include_directories(megamanx4_port PRIVATE ${X4_TITLE_DIRS})

target_compile_options(megamanx4_port PRIVATE -O2 -g
  ${SDL3_CFLAGS_OTHER} ${FREETYPE_CFLAGS_OTHER})
