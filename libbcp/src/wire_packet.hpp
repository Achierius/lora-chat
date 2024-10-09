#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <functional>

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

#include "packet.hpp"

namespace lora_chat {

constexpr size_t kWirePacketTagBits = 8;
static_assert((1 << kWirePacketTagBits) >=
              static_cast<size_t>(kFinalPacketType));

constexpr size_t kWirePacketTagBytes = ((kWirePacketTagBits + 7) / 8);

template <PacketType Pt, size_t Index = 0>
constexpr std::size_t WirePacketWidthSansTag() {
  using Packet = Packet<Pt>;
  using Field = typename Packet::Field;
  if constexpr (Index == static_cast<std::size_t>(Packet::kFinalField)) {
    return Packet::FieldMetadata(Packet::kFinalField).starting_bit + Packet::FieldMetadata(Packet::kFinalField).length_bits;
  } else {
    constexpr Field field = static_cast<Field>(Index);
    constexpr std::size_t value =
       Packet::FieldMetadata(field).starting_bit + Packet::FieldMetadata(field).length_bits;
    return (value > WirePacketWidthSansTag<Pt, Index + 1>())
               ? value
               : WirePacketWidthSansTag<Pt, Index + 1>();
  }
}

template <PacketType Pt>
constexpr std::size_t WirePacketWidth() {
  return WirePacketWidthSansTag<Pt>() + kWirePacketTagBits;
}

template <PacketType Pt>
constexpr std::size_t WirePacketWidthBytes() {
  return (WirePacketWidth<Pt>() + 7) / 8;
}

template <PacketType Pt>
using NewWirePacket = std::array<uint8_t, WirePacketWidthBytes<Pt>()>;
using WireSessionPacket = NewWirePacket<PacketType::kSession>;
using WireAdvertisingPacket = NewWirePacket<PacketType::kAdvertising>;

struct __attribute__ ((packed)) ReceiveBuffer {
  using value_type = uint8_t;
  std::array<value_type, SX127x_FIFO_CAPACITY> buffer;

  value_type* data() { return buffer.data(); }
  const value_type* data() const { return buffer.data(); }
  size_t size() const { return buffer.size(); }

  std::span<uint8_t> span() { // TODO this is gross
    return {reinterpret_cast<uint8_t*>(this), sizeof(*this) / sizeof(uint8_t)};
  }
};

template <PacketType Pt>
constexpr bool AllFieldInvariantsAreSatisfied() {
  using Flag = PacketFieldFlags;
  using PacketT = Packet<Pt>;
  using Field = typename PacketT::Field;

  // Check overlaps and overextensions
  for (int i = 0; i <= static_cast<long>(PacketT::kFinalField); i++) {
    auto f1 = static_cast<Field>(i);
    auto f1_m = PacketT::FieldMetadata(f1);
    // Check if f1 extends past the end of the packet
    if (f1_m.starting_bit + f1_m.length_bits > WirePacketWidth<Pt>())
      return false;
    // Check if any fields are not byte aligned -- this will go away later
    if (f1_m.starting_bit % 8 || f1_m.length_bits % 8)
      return false;
    // Check for intersections
    if (f1_m.flags & Flag::kMayOverlap) continue;
    for (int j = 0; j <= static_cast<long>(PacketT::kFinalField); j++) {
      if (i == j) continue;

      auto f2 = static_cast<Field>(j);
      auto f2_m = PacketT::FieldMetadata(f2);
      if (f2_m.flags & kMayOverlap) continue;

      // Check whether f1 protrudes onto f2 from below
      if ((f1_m.starting_bit <= f2_m.starting_bit) &&
          ((f1_m.starting_bit + f1_m.length_bits) > f2_m.starting_bit))
        return false;
    }
  }
  return true;
}

// TODO make this iterate over all packet-types rather than hardcoding instances
static_assert(AllFieldInvariantsAreSatisfied<PacketType::kSession>());
static_assert(AllFieldInvariantsAreSatisfied<PacketType::kAdvertising>());
static_assert(WirePacketWidthBytes<PacketType::kSession>() <= SX127x_FIFO_CAPACITY);
static_assert(WirePacketWidthBytes<PacketType::kAdvertising>() <= SX127x_FIFO_CAPACITY);
static_assert(static_cast<size_t>(kFinalPacketType) == 1);  // As a reminder
                                                            // until I
                                                            // un-hard-code this

template <PacketType Pt>
NewWirePacket<Pt> Serialize(Packet<Pt> const &packet) {
  using Field = typename Packet<Pt>::Field;
  constexpr Field kFinalField = Packet<Pt>::kFinalField;
  constexpr auto kBitOffset = kWirePacketTagBits;

  NewWirePacket<Pt> buffer{};

  // TODO allow non-byte-sized tags
  static_assert((kWirePacketTagBits % 8) == 0);
  const PacketType tag {Pt};
  std::memcpy(&buffer[0], &tag, sizeof(tag));

  // All fields are byte-aligned for now so just do a simple copy
  for (size_t i = 0; i <= static_cast<size_t>(kFinalField); i++) {
    auto f = static_cast<Field>(i);
    auto m = packet.FieldMetadata(f);
    uint8_t const* src = packet.GetFieldPointer(f);
    assert((m.starting_bit + kBitOffset) % 8 == 0);  // TODO allow
                                                     // non-byte-aligned tags
    uint8_t* dst = &buffer[(m.starting_bit + kBitOffset) / 8];
    std::memcpy(dst, src, m.length_bits / 8);
  }

  return buffer;
}

// TODO will take some work to handle when fields are no longer byte-aligned
template <PacketType Pt>
void VisualizeSerializationLayout(std::ostream& os) {
  using Field = typename Packet<Pt>::Field;
  constexpr Field kFinalField = Packet<Pt>::kFinalField;
  constexpr auto kBitOffset = kWirePacketTagBits;

  // XX YY ZZ: 2 chars per fieldbyte, plus #fieldbytes-1 spaces inbetween
  const auto kSerializedBytes = WirePacketWidthBytes<Pt>();
  const auto kOutputStringLength = (2 * kSerializedBytes) + (kSerializedBytes - 1);
  std::string buff(kOutputStringLength, ' ');

  static_assert(kWirePacketTagBits == 8);
  buff[0] = 'T';
  buff[1] = 'G';

  const int kLettersInAlphabet = 'Z' - 'A' + 1;
  for (size_t i = 0; i <= static_cast<size_t>(kFinalField); i++) {
    char field_prefix = ('F' + (i / kLettersInAlphabet));
    char field_suffix = ('A' + (i % kLettersInAlphabet));

    auto f = static_cast<Field>(i);
    auto m = decltype(Packet<Pt>())::FieldMetadata(f);

    assert(!((m.starting_bit + kBitOffset) % 8));
    assert(!(m.length_bits % 8));
    assert(m.length_bits / 8);
    size_t start_ser_byte = (m.starting_bit + kBitOffset) / 8;
    size_t num_ser_bytes = m.length_bits / 8;
    size_t start_str_byte = (3 * start_ser_byte) - 1;
    size_t end_str_byte = start_str_byte + (3 * num_ser_bytes);
    assert(end_str_byte >= start_str_byte);
    for (size_t b = start_str_byte; b < end_str_byte; b += 3) {
      buff[b] = ' ';
      buff[b + 1] = field_prefix;
      buff[b + 2] = field_suffix;
    }
  }

  os << buff;
}

template <PacketType Pt>
std::optional<Packet<Pt>> DeserializeImpl(std::span<uint8_t const> bytes) {
  using Packet = Packet<Pt>;
  using Field = typename Packet::Field;
  constexpr Field kFinalField = Packet::kFinalField;
  constexpr auto kBitOffset = kWirePacketTagBits;

  if (bytes.size_bytes() < kWirePacketTagBytes)
    return {};

  PacketType tag{};
  std::memcpy(&tag, &bytes[0], sizeof(tag));
  if (tag != Pt || bytes.size_bytes() < WirePacketWidthBytes<Pt>())
    return {};

  Packet packet{};

  // All fields are byte-aligned for now so just do a simple copy
  for (size_t i = 0; i <= static_cast<size_t>(Packet::kFinalField); i++) {
    auto f = static_cast<Field>(i);
    auto m = Packet::FieldMetadata(f);
    assert((m.starting_bit + kBitOffset) % 8 == 0);  // TODO allow
                                                     // non-byte-aligned tags
    uint8_t const* src = &bytes[(m.starting_bit + kBitOffset) / 8];
    uint8_t* dst = packet.GetFieldPointer(f);
    std::memcpy(dst, src, m.length_bits / 8);
  }

  return packet;
}

template <PacketType Pt>
std::optional<Packet<Pt>> Deserialize(ReceiveBuffer bytes) {
  return DeserializeImpl<Pt>(bytes.span());
}

} // namespace lora_chat
