#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace lora_chat {

using WireSessionId = uint64_t;
using WireSessionTime = uint64_t;
using SequenceNumber = uint8_t; // bluetooth uses 1 bit, but how does that
                                // handle time-delayed reflections?
constexpr size_t kMaxPayloadLengthBytes = 32;  // could be longer
constexpr size_t kPacketSizeBytes = kMaxPayloadLengthBytes + 24;

enum class PacketField {
  kSessionId = 0,
  kNesn,
  kSn,
  kLength,
  kPayload, // must be last field
};

/// The physical packet which we send across the radio channel.
using WirePacket = std::array<uint8_t, kPacketSizeBytes>;

/// A logical representation of a packet in memory.
/// Must be serialized in order to be sent over the wire.
struct Packet {
  WireSessionId id;
  SequenceNumber nesn;
  SequenceNumber sn;
  uint8_t length;
  std::array<uint8_t, kMaxPayloadLengthBytes> payload;

  // The actual wire format is described in the .cpp file
  // TODO replace all uses of uint8_t with std::byte everywhere
  WirePacket Serialize() const;
  static Packet Deserialize(std::span<uint8_t const> buffer);
  void DeserializeInto(std::span<uint8_t const> buffer);
};

inline bool operator==(const Packet& lhs, const Packet& rhs) {
  return (lhs.id == rhs.id && lhs.nesn == rhs.nesn && lhs.sn == rhs.sn && lhs.length == rhs.length && lhs.payload == rhs.payload);
}

} // namespace lora_chat
