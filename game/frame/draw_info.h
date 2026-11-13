// The two retail DRAWINFO buffers of gameMain: the one being built is at kCurrentDrawInfo.
#pragma once

#include <cstdint>

namespace x4::draw_info {

inline constexpr std::uint32_t kCurrentDrawInfo = 0x80142F80u;
inline constexpr std::uint32_t kDrawInfos = 0x80166C10u;
inline constexpr std::uint32_t kDrawInfoStride = 160u;
inline constexpr std::uint32_t kDrawEnvOffset = 20u;
// ClearOTagR(drawInfo + 0x70, 12): slot d is drawInfo + 0x70 + d * 4; DrawOTag starts at the last.
inline constexpr std::uint32_t kOrderingTableClearOffset = 112u;
inline constexpr std::uint32_t kOrderingTableSlots = 12u;
inline constexpr std::uint32_t kOrderingTableDrawOffset = kOrderingTableClearOffset + (kOrderingTableSlots - 1u) * 4u;

constexpr std::uint32_t orderingTableRoot(std::uint32_t drawInfo) {
  return drawInfo + kOrderingTableDrawOffset;
}
constexpr std::uint32_t orderingTableSlot(std::uint32_t drawInfo, std::uint32_t depth) {
  return drawInfo + kOrderingTableClearOffset + depth * 4u;
}

} // namespace x4::draw_info
