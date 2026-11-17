// SPDX-License-Identifier: AGPL-3.0-or-later
// sequence_skip.h - Start skips a dialogue by feeding the game its own advance input.
// Every dialogue box runs through one update, 0x80022730 (the Hunter H.Q. briefing 0x8002F4C4, engine state 3
// sub-state 10 0x8002F698, and in a stage 0x80021D20): a page types out, then waits for a face-button edge. While
// Start is held the Cross edge is raised on a fixed field cadence, so the retail owner runs every page to its end.
#pragma once

#include <cstdint>

class Core;

namespace x4::sequence_skip {

// abc_object (0x8013BC28): +0xC is nonzero while a dialogue runs (0x8002217C sets it, 0x80022730 clears it).
inline constexpr std::uint32_t kDialogueActive = 0x8013BC34u;

// Stage sub-state 2 is the pause menu (0x80020368): it reads the pad itself (0x80035EF0) and the dialogue update does
// not run, so a raised Cross would pick a menu row.
inline constexpr std::uint8_t kPauseMenu = 2;

// Guest button words (`~packet`, bytes 2-3 of the libpad record): Start is bit 3 of byte 2, Cross bit 6 of byte 3.
inline constexpr std::uint16_t kStartMask = 0x0800u;
inline constexpr std::uint16_t kCrossMask = 0x0040u;

// A page finishes typing and then waits for the edge, so a held Cross would not advance; one edge per cadence does.
inline constexpr std::uint32_t kAdvanceCadence = 4;

class SequenceSkip {
public:
  // A dialogue is up and its update runs this field.
  static bool inDialogue(Core &core);

  // Runs after the retail pad route of every field and rewrites its Cross held/edge words while skipping.
  void afterPadRoute(Core &core);

  std::uint32_t fieldsSkipped() const {
    return fields_;
  }

private:
  std::uint32_t fields_ = 0;
};

} // namespace x4::sequence_skip
