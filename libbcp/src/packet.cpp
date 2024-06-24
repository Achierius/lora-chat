#include "packet.hpp"

#include <cassert>
#include <cstring>

namespace lora_chat {

enum PacketFieldFlags : uint32_t {
  kNone = 0,
  kMayOverlap = 1,
  kZeroEncodesMax = 2,
};

struct PacketFieldInfo {
  size_t starting_bit;
  size_t length_bits;
  PacketFieldFlags flags;
};

namespace {

constexpr PacketFieldInfo FieldMetadata(PacketField f) {
  using Flag = PacketFieldFlags;
  using Field = PacketField;
  switch (f) {
  case Field::kSessionId:
    return {0, 64, Flag::kNone};
  case Field::kNesn:
    return {64, 8, Flag::kNone};
  case Field::kSn:
    return {72, 8, Flag::kNone};
  case Field::kLength:
    return {80, 8, Flag::kNone};  // TODO impl kZeroEncodesMax
  case Field::kPayload:
    return {88, kMaxPayloadLengthBytes * 8, Flag::kNone};
  }
  __builtin_trap();
}

constexpr bool AllFieldInvariantsAreSatisfied() {
  using Flag = PacketFieldFlags;
  using Field = PacketField;
  // Check overlaps and overextensions
  for (int i = 0; i <= static_cast<long>(Field::kPayload); i++) {
    auto f1 = static_cast<Field>(i);
    auto f1_m = FieldMetadata(f1);
    // Check if f1 extends past the end of the packet
    if (f1_m.starting_bit + f1_m.length_bits > (kPacketSizeBytes * 8))
      return false;
    // Check if any fields are not byte aligned -- this will go away later
    if (f1_m.starting_bit % 8 || f1_m.length_bits % 8)
      return false;
    // Check for intersections
    if (f1_m.flags & Flag::kMayOverlap) continue;
    for (int j = 0; j <= static_cast<long>(Field::kPayload); j++) {
      if (i == j) continue;

      auto f2 = static_cast<Field>(j);
      auto f2_m = FieldMetadata(f2);
      if (f2_m.flags & kMayOverlap) continue;

      // Check whether f1 protrudes onto f2 from below
      if ((f1_m.starting_bit <= f2_m.starting_bit) &&
          ((f1_m.starting_bit + f1_m.length_bits) > f2_m.starting_bit))
        return false;
    }
  }
  return true;
}

static_assert(AllFieldInvariantsAreSatisfied());

uint8_t const* GetPointerToPacketField(Packet const& p, PacketField f) {
  switch (f) {
  case PacketField::kSessionId:
    return reinterpret_cast<uint8_t const*>(&(p.id));
  case PacketField::kNesn:
    return reinterpret_cast<uint8_t const*>(&(p.nesn));
  case PacketField::kSn:
    return reinterpret_cast<uint8_t const*>(&(p.sn));
  case PacketField::kLength:
    return reinterpret_cast<uint8_t const*>(&(p.length));
  case PacketField::kPayload:
    return reinterpret_cast<uint8_t const*>(&(p.payload));
  }
  __builtin_trap();
}

uint8_t* GetPointerToPacketField(Packet& p, PacketField f) {
  // this is legal because we know that the original type is const
  return const_cast<uint8_t*>(GetPointerToPacketField(const_cast<Packet const&>(p), f));
}

}

WirePacket Packet::Serialize() const {
  using Field = PacketField;

  WirePacket buffer{};

  // All fields are byte-aligned for now so just do a simple copy
  for (int i = 0; i <= static_cast<long>(Field::kPayload); i++) {
    auto f = static_cast<Field>(i);
    auto m = FieldMetadata(f);
    uint8_t const* src = GetPointerToPacketField(*this, f);
    uint8_t* dst = &buffer[m.starting_bit / 8];
    std::memcpy(dst, src, m.length_bits / 8);
  }

  return buffer;
}

Packet Packet::Deserialize(std::span<uint8_t const> buffer) {
  Packet p {};
  p.DeserializeInto(buffer);
  return p;
}

void Packet::DeserializeInto(std::span<uint8_t const> buffer) {
  assert(buffer.size_bytes() >= kPacketSizeBytes);
  using Field = PacketField;

  // All fields are byte-aligned for now so just do a simple copy
  for (int i = 0; i <= static_cast<long>(Field::kPayload); i++) {
    auto f = static_cast<Field>(i);
    auto m = FieldMetadata(f);
    uint8_t const* src = &buffer[m.starting_bit / 8];
    uint8_t* dst = GetPointerToPacketField(*this, f);
    std::memcpy(dst, src, m.length_bits / 8);
  }
}

}
