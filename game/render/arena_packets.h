// SPDX-License-Identifier: AGPL-3.0-or-later
// arena_packets.h - the SLUS_005.61 scratchpad packet arenas the object draw pass (0x80023DB8) fills, and moving
// the packets a pass wrote into them when they are drawn.
#pragma once

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace x4::arena {

// Scratchpad next-free pointers of the packet arenas, set @ 0x80023DF4..0x80023E5C: polygons and sprites (0x100),
// draw modes (0x104), sprites (0x108), draw modes (0x10C) and flat quads (0x110, the gauge bars @ 0x80025588).
inline constexpr std::array<std::uint32_t, 5> kCursors = {
    0x1F800100u, 0x1F800104u, 0x1F800108u, 0x1F80010Cu, 0x1F800110u};

using Positions = std::array<std::uint32_t, kCursors.size()>;

// A GP0 packet in an arena, by its guest address.
struct Packet {
  std::uint32_t address;
};

Positions positions(Core &core);
// The packet after `packet` in its arena, from its tag's word count.
Packet nextPacket(Core &core, Packet packet);
// The leftmost vertex x of a polygon or rectangle packet; none for any other command.
std::optional<int> leftmostX(Core &core, Packet packet);
// The walk of the table being built draws `packet` with every vertex `dx` columns right; guest RAM is untouched.
void moveX(Core &core, Packet packet, int dx);

// The object draws (0x80024334, 0x8002588C, 0x80024920, 0x80024B9C) put each piece in a slot of this many bytes of
// the first arena whatever its command's length (cursor `+0x28` per piece); its draw mode goes to the second.
inline constexpr std::uint32_t kObjectSlotBytes = 0x28u;

// Calls `visit(packet)` for each object piece the first arena received since `before`.
template <typename Visit> void forEachObjectPieceSince(Core &core, const Positions &before, Visit visit) {
  const std::uint32_t end = positions(core)[0];
  for (Packet packet{before[0]}; packet.address < end; packet.address += kObjectSlotBytes) {
    visit(packet);
  }
}

// Calls `visit(packet)` for each packet the arenas received since `before`, stepping by each tag's length.
template <typename Visit> void forEachSince(Core &core, const Positions &before, Visit visit) {
  const Positions after = positions(core);
  for (std::size_t i = 0; i < kCursors.size(); ++i) {
    for (Packet packet{before[i]}; packet.address < after[i]; packet = nextPacket(core, packet)) {
      visit(packet);
    }
  }
}

} // namespace x4::arena
