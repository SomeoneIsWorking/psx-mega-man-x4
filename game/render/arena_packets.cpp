// SPDX-License-Identifier: AGPL-3.0-or-later
#include "arena_packets.h"

#include "core.h"

#include <algorithm>

namespace x4::arena {
namespace {

// GP0 packet layout: tag word, then the command byte at +7; rectangles keep xy at +8.
constexpr std::uint32_t kTagLengthOffset = 3;
constexpr std::uint32_t kCommandOffset = 7;
constexpr std::uint32_t kFirstVertexOffset = 8;

constexpr std::uint8_t kPolygonClass = 0x20;
constexpr std::uint8_t kRectangleClass = 0x60;
constexpr std::uint8_t kClassMask = 0xE0;
constexpr std::uint8_t kQuadBit = 0x08;
constexpr std::uint8_t kTexturedBit = 0x04;
constexpr std::uint8_t kGouraudBit = 0x10;

struct Vertices {
  std::uint32_t count;
  std::uint32_t stride;
};

// Polygon vertices follow the colour word; a shaded vertex is preceded by its colour, a textured one followed by uv.
Vertices verticesOf(std::uint8_t command) {
  const std::uint8_t kind = command & kClassMask;
  if (kind == kRectangleClass) {
    return {1u, 0u};
  }
  if (kind != kPolygonClass) {
    return {0u, 0u};
  }
  const std::uint32_t count = (command & kQuadBit) != 0 ? 4u : 3u;
  const std::uint32_t stride =
      4u + ((command & kTexturedBit) != 0 ? 4u : 0u) + ((command & kGouraudBit) != 0 ? 4u : 0u);
  return {count, stride};
}

std::uint32_t vertexX(Packet packet, const Vertices &vertices, std::uint32_t i) {
  return packet.address + kFirstVertexOffset + i * vertices.stride;
}

std::uint8_t commandOf(Core &core, Packet packet) {
  return core.mem_r8(packet.address + kCommandOffset);
}

} // namespace

Positions positions(Core &core) {
  Positions positions{};
  for (std::size_t i = 0; i < kCursors.size(); ++i) {
    positions[i] = core.mem_r32(kCursors[i]);
  }
  return positions;
}

Packet nextPacket(Core &core, Packet packet) {
  return {packet.address + (static_cast<std::uint32_t>(core.mem_r8(packet.address + kTagLengthOffset)) + 1u) * 4u};
}

std::optional<int> leftmostX(Core &core, Packet packet) {
  const Vertices vertices = verticesOf(commandOf(core, packet));
  std::optional<int> leftmost;
  for (std::uint32_t i = 0; i < vertices.count; ++i) {
    const int x = static_cast<std::int16_t>(core.mem_r16(vertexX(packet, vertices, i)));
    leftmost = leftmost ? std::min(*leftmost, x) : x;
  }
  return leftmost;
}

void shiftX(Core &core, Packet packet, int dx) {
  const Vertices vertices = verticesOf(commandOf(core, packet));
  for (std::uint32_t i = 0; i < vertices.count; ++i) {
    const std::uint32_t address = vertexX(packet, vertices, i);
    core.mem_w16(address, static_cast<std::uint16_t>(static_cast<std::int16_t>(core.mem_r16(address)) + dx));
  }
}

} // namespace x4::arena
