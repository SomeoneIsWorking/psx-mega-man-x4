// counterpart.h — the other hunter's resources for player 2: its character archive and the stage's palette for
// it, read from the player's disc, lent to the resident character's member slots only while player 2 is projected.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class Core;

namespace x4::coop {

// Resident archive members the counterpart lends into, each found through its scratch resource word
// (0x80035240, 0x800350A4): effects 6/7, assembly 9, portrait 15. The body (member 2) is lent per decompression;
// members 3, 8 and 14 are byte-equal in both archives. Colors (member 0) are not lent: a pass that reloads them
// writes the character CLUT rows player 1 draws with, and player 2's come from the stage palette instead.
struct LentMember {
  std::uint32_t member;
  std::uint32_t scratchWord;
};
inline constexpr std::array<LentMember, 4> kLentMembers{{
    {6u, 0x1F800034u},
    {7u, 0x1F800038u},
    {9u, 0x1F80001Cu},
    {15u, 0x1F800048u},
}};
inline constexpr std::uint32_t kBodyMember = 2u;
inline constexpr std::uint32_t kBodyScratchWord = 0x1F800014u;
// The stage palette member: 256 x 8 CLUT halfwords for rows 480-487.
inline constexpr std::uint32_t kPaletteMember = 0u;
inline constexpr std::uint32_t kPaletteType = 5u;
inline constexpr std::uint32_t kPaletteBytes = 0x1000u;

class Counterpart final {
public:
  // Reads both characters' archives and the stage palette `ARC/COLss_pX.ARC`; the error names what is missing.
  std::optional<std::string>
  load(Core &core, std::uint8_t resident, std::uint8_t other, std::uint8_t stage, std::uint8_t section);
  bool loaded() const {
    return loaded_;
  }

  // Exchanges the lent members with the resident ones in guest memory; two calls restore them.
  void exchange(Core &core);

  // Animation `index` of the blob the body's header word `table` names.
  struct FrameKey {
    std::uint32_t table;
    std::uint32_t index;
  };
  // The counterpart's animation word and compressed stream for `key`; nullopt when it is out of the table.
  struct Frame {
    std::uint32_t word;
    std::vector<std::uint8_t> stream;
  };
  std::optional<Frame> frame(FrameKey key) const;
  // Which header word of the resident body `blobOffset` is, or nullopt.
  std::optional<std::uint32_t> residentTable(std::uint32_t blobOffset) const;

  const std::vector<std::uint8_t> &palette() const {
    return palette_;
  }

private:
  std::array<std::vector<std::uint8_t>, kLentMembers.size()> lent_;
  std::vector<std::uint8_t> residentBody_;
  std::vector<std::uint8_t> body_;
  std::vector<std::uint8_t> palette_;
  bool loaded_ = false;
};

} // namespace x4::coop
