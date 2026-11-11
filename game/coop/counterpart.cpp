// ARC layout adapted from MegaManX4Recomp `src/mods/mmx4_coop/assets.c` (MIT, mstan).
#include "counterpart.h"

#include "core.h"
#include "disc_file.h"
#include "executable_write_source.h"
#include "game.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <lucent/log.h>
#include <span>

namespace x4::coop {
namespace {

// ARC: u32 member count, u32 file size, then (u32 type, u32 bytes) per member; members from 0x800, each padded to
// a sector.
constexpr std::size_t kArcSector = 0x800u;
constexpr std::uint32_t kArcMaxMembers = 255u;

std::uint32_t le32(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

struct Member {
  std::uint32_t type = 0;
  std::span<const std::uint8_t> bytes;
};

std::optional<std::vector<Member>> arcMembers(std::span<const std::uint8_t> file) {
  if (file.size() < kArcSector || le32(file, 4) != file.size()) {
    return std::nullopt;
  }
  const std::uint32_t count = le32(file, 0);
  if (count > kArcMaxMembers) {
    return std::nullopt;
  }
  std::vector<Member> members;
  std::size_t cursor = kArcSector;
  for (std::uint32_t index = 0; index < count; ++index) {
    const std::size_t bytes = le32(file, 12 + index * 8u);
    if (cursor > file.size() || bytes > file.size() - cursor) {
      return std::nullopt;
    }
    members.push_back({.type = le32(file, 8 + index * 8u), .bytes = file.subspan(cursor, bytes)});
    cursor += (bytes + kArcSector - 1u) & ~(kArcSector - 1u);
  }
  if (cursor != file.size()) {
    return std::nullopt;
  }
  return members;
}

std::string archivePath(std::uint8_t character) {
  return lucent::format("ARC/PL{:02}_U.ARC", character);
}

std::vector<std::uint8_t> copy(std::span<const std::uint8_t> bytes) {
  return {bytes.begin(), bytes.end()};
}

} // namespace

std::optional<std::string>
Counterpart::load(Core &core, std::uint8_t resident, std::uint8_t other, std::uint8_t stage, std::uint8_t section) {
  loaded_ = false;
  identity_.reset();
  psx::cd::Disc &disc = core.game->disc;
  std::vector<std::uint8_t> residentFile;
  std::vector<std::uint8_t> otherFile;
  std::vector<std::uint8_t> paletteFile;
  const std::string residentPath = archivePath(resident);
  const std::string otherPath = archivePath(other);
  const std::string palettePath = lucent::format("ARC/COL{:02X}_{}{}.ARC", stage, section, other == 0u ? 'X' : 'Z');
  for (const auto &[path, out] : {std::pair{&residentPath, &residentFile},
                                  std::pair{&otherPath, &otherFile},
                                  std::pair{&palettePath, &paletteFile}}) {
    if (!psx::cd::readDiscFile(disc, path->c_str(), *out)) {
      return lucent::format("{} is not readable on the disc", *path);
    }
  }
  const std::optional<std::vector<Member>> residentMembers = arcMembers(residentFile);
  const std::optional<std::vector<Member>> otherMembers = arcMembers(otherFile);
  const std::optional<std::vector<Member>> paletteMembers = arcMembers(paletteFile);
  if (!residentMembers || !otherMembers || !paletteMembers) {
    return lucent::format("{}, {} or {} is not an archive", residentPath, otherPath, palettePath);
  }
  if (paletteMembers->empty() || (*paletteMembers)[kPaletteMember].type != kPaletteType ||
      (*paletteMembers)[kPaletteMember].bytes.size() != kPaletteBytes) {
    return lucent::format("{} member 0 is not a {}-byte palette", palettePath, kPaletteBytes);
  }
  for (std::size_t index = 0; index < kLentMembers.size(); ++index) {
    const std::uint32_t member = kLentMembers[index].member;
    if (member >= residentMembers->size() || member >= otherMembers->size()) {
      return lucent::format("{} has no member {}", otherPath, member);
    }
    const std::span<const std::uint8_t> own = (*residentMembers)[member].bytes;
    const std::span<const std::uint8_t> lent = (*otherMembers)[member].bytes;
    if (lent.size() > own.size()) {
      return lucent::format(
          "{} member {} is {} bytes, larger than the resident {}", otherPath, member, lent.size(), own.size());
    }
    lent_[index] = copy(lent);
  }
  if (kUiMember >= otherMembers->size() || (*otherMembers)[kUiMember].type != kUiType ||
      (*otherMembers)[kUiMember].bytes.size() != kUiStrips * kArcSector) {
    return lucent::format("{} member {} is not the {}-strip UI texture", otherPath, kUiMember, kUiStrips);
  }
  ui_ = copy((*otherMembers)[kUiMember].bytes);
  residentBody_ = copy((*residentMembers)[kBodyMember].bytes);
  body_ = copy((*otherMembers)[kBodyMember].bytes);
  palette_ = copy((*paletteMembers)[kPaletteMember].bytes);
  identity_ = Identity{.resident = resident, .stage = stage, .section = section};
  loaded_ = true;
  return std::nullopt;
}

void Counterpart::exchange(Core &core) {
  std::vector<std::byte> guest;
  for (std::size_t index = 0; index < kLentMembers.size(); ++index) {
    std::vector<std::uint8_t> &lent = lent_[index];
    const std::uint32_t address = core.mem_r32(kLentMembers[index].scratchWord);
    guest.resize(lent.size());
    core.readBytes(address, guest);
    core.mem_write(address, lent, psx::cpu::ExecutableWriteSource::Native);
    std::memcpy(lent.data(), guest.data(), lent.size());
  }
}

// The strip order is MegaManX4Recomp's `retain_ui` (layout 2: each column from y 176 to 240).
std::optional<std::uint16_t> Counterpart::uiPixel(std::int32_t x, std::int32_t y) const {
  const std::int32_t column = (x - kUiLeft) / kUiStripWidth;
  const std::int32_t strip = column * kUiStripsPerColumn + (y - kUiTop) / kUiStripRows;
  if (x < kUiLeft || y < kUiTop || strip >= static_cast<std::int32_t>(kUiStrips) ||
      y >= kUiTop + kUiStripsPerColumn * kUiStripRows) {
    return std::nullopt;
  }
  const std::size_t offset =
      static_cast<std::size_t>(strip) * kArcSector +
      static_cast<std::size_t>(((y - kUiTop) % kUiStripRows) * kUiStripWidth + (x - kUiLeft) % kUiStripWidth) * 2u;
  return static_cast<std::uint16_t>(ui_[offset] | (ui_[offset + 1u] << 8));
}

std::optional<std::uint32_t> Counterpart::residentTable(std::uint32_t blobOffset) const {
  for (std::size_t table = 0; table < 2u; ++table) {
    if ((table + 1u) * 4u <= residentBody_.size() && le32(residentBody_, table * 4u) == blobOffset) {
      return static_cast<std::uint32_t>(table);
    }
  }
  return std::nullopt;
}

std::optional<Counterpart::Frame> Counterpart::frame(FrameKey key) const {
  constexpr std::uint32_t kOffsetMask = 0xFFFFFu;
  const std::size_t table = key.table;
  const std::size_t index = key.index;
  if ((table + 1u) * 4u > body_.size()) {
    return std::nullopt;
  }
  const std::span<const std::uint8_t> body(body_);
  const std::size_t blob = le32(body, table * 4u);
  if (blob + (index + 1u) * 4u > body.size()) {
    return std::nullopt;
  }
  const std::uint32_t word = le32(body, blob + index * 4u);
  const std::size_t start = blob + (word & kOffsetMask);
  if (start >= body.size()) {
    return std::nullopt;
  }
  // The stream runs to the next stream of the same table, or to the body's end.
  std::size_t end = body.size();
  std::size_t tableEnd = end;
  for (std::size_t entry = blob; entry + 4u <= tableEnd; entry += 4u) {
    const std::size_t other = blob + (le32(body, entry) & kOffsetMask);
    tableEnd = std::min(tableEnd, other);
    if (other > start && other < end) {
      end = other;
    }
  }
  return Frame{.word = word, .stream = copy(body.subspan(start, end - start))};
}

} // namespace x4::coop
