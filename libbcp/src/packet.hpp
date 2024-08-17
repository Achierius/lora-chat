#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "sequence_number.hpp"

namespace lora_chat {

using WireSessionId = uint32_t;
using WirePacketType = uint8_t;
using WireSessionTime = uint64_t;
using WireSequenceNumber = uint8_t;
using WirePayloadLength = uint8_t;
constexpr size_t kMaxPayloadLengthBytes = 32; // could be longer
constexpr size_t kPacketSizeBytes =
    kMaxPayloadLengthBytes + sizeof(WireSessionId) + sizeof(WirePacketType) +
    sizeof(WirePayloadLength) + (2 * sizeof(WireSequenceNumber));

// The physical packet which we send across the radio channel.
using WirePacket = std::array<uint8_t, kPacketSizeBytes>;
// The payload of the packet.
using WirePacketPayload = std::array<uint8_t, kMaxPayloadLengthBytes>;

enum class PacketField {
  kSessionId = 0,
  kType,
  kLength,
  kNesn,
  kSn,
  kPayload, // must be last field
};

/// A logical representation of a packet in memory.
/// Must be serialized in order to be sent over the wire.
struct Packet {
  // TODO make this a proper sum type
  // TODO make this printable
  enum Type {
    // TODO 0 should be invalid; keeping it this way for ease of testing
    kNack = 0,
    kData = 1,
    kAdvertisement = 2,
    kConnectionRequest = 3,
  };

  WireSessionId id;
  Type type;
  WirePayloadLength length;
  SequenceNumber nesn;
  SequenceNumber sn;
  WirePacketPayload payload;

  // The actual wire format is described in the .cpp file
  // TODO replace all uses of uint8_t with std::byte everywhere
  WirePacket Serialize() const;
  static Packet Deserialize(std::span<uint8_t const> buffer);
  void DeserializeInto(std::span<uint8_t const> buffer);
};

inline bool operator==(const Packet &lhs, const Packet &rhs) {
  return (lhs.id == rhs.id && lhs.type == rhs.type &&
          lhs.length == rhs.length && lhs.nesn == rhs.nesn &&
          lhs.sn == rhs.sn && lhs.payload == rhs.payload);
}

} // namespace lora_chat
