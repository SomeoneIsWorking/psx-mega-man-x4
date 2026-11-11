#include "player_two.h"

#include "core.h"

#include "executable_write_source.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>

namespace x4::coop {
namespace {

constexpr std::size_t contextSize() {
  std::size_t total = 0;
  for (const Block &block : kPlayerBlocks) {
    total += block.size;
  }
  return total;
}

constexpr std::uint32_t largestBlock() {
  std::uint32_t largest = 0;
  for (const Block &block : kPlayerBlocks) {
    largest = std::max(largest, block.size);
  }
  return largest;
}
constexpr std::uint32_t kLargestBlock = largestBlock();

} // namespace

PlayerTwo::PlayerTwo() : bytes_(contextSize(), 0u) {}

void PlayerTwo::exchange(Core &core) {
  std::size_t cursor = 0;
  std::array<std::byte, kLargestBlock> guest{};
  for (const Block &block : kPlayerBlocks) {
    const std::span<std::byte> held(guest.data(), block.size);
    core.readBytes(block.address, held);
    const std::span<std::uint8_t> own(bytes_.data() + cursor, block.size);
    core.mem_write(block.address, own, psx::cpu::ExecutableWriteSource::Native);
    std::memcpy(own.data(), held.data(), block.size);
    cursor += block.size;
  }
}

void PlayerTwo::clear() {
  bytes_.assign(bytes_.size(), 0u);
}

std::uint8_t PlayerTwo::body8(std::uint32_t offset) const {
  return bytes_[offset];
}

std::int16_t PlayerTwo::body16(std::uint32_t offset) const {
  return static_cast<std::int16_t>(bytes_[offset] | (bytes_[offset + 1] << 8));
}

void PlayerTwo::setLife(std::uint8_t life) {
  bytes_[kLife] = static_cast<std::uint8_t>((bytes_[kLife] & 0x80u) | (life & 0x7Fu));
}

bool PlayerTwo::alive() const {
  return body8(0) != 0u && (body8(kLife) & 0x7Fu) != 0u && body8(kState) < kFirstDeadState;
}

} // namespace x4::coop
