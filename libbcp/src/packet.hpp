#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <span>

#include "time.hpp"
#include "sequence_number.hpp"
#include "sx1276/sx1276.hpp"

namespace lora_chat {

using WireSessionId = uint32_t;
using WireAddress = uint32_t;
using WireSequenceNumber = uint8_t;
using WirePayloadLength = uint8_t;

constexpr size_t kSessionPacketPayloadBytes = 32;
using SessionPacketPayload = std::array<uint8_t, kSessionPacketPayloadBytes>;

enum class PacketType : uint8_t {
  kSession = 0,
  kConnectionRequest,
  kConnectionAccept,
  kAdvertising,
};
constexpr PacketType kFinalPacketType = PacketType::kAdvertising;

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

/// A logical representation of a packet in memory.
/// Must be serialized in order to be sent over the wire.
template <PacketType Pt>
struct Packet;

template <>
struct Packet<PacketType::kSession> {
  static constexpr PacketType kType = PacketType::kSession;

  enum class Field {
    kSessionId = 0,
    kType,
    kLength,
    kNesn,
    kSn,
    kPayload, // must be last field
  };

  enum SubType {
    // TODO 0 should be invalid; keeping it this way for ease of testing
    kNack = 0,
    kData = 1,
    kConnectionRequest = 3,
    kConnectionAccept = 4,
  };

  static constexpr Field kFinalField = Field::kPayload;

  static constexpr PacketFieldInfo FieldMetadata(Field f) {
    using Flag = PacketFieldFlags;
    switch (f) {
    case Field::kSessionId:
      return {0, 32, Flag::kNone};
    case Field::kType:
      return {32, 8, Flag::kNone};
    case Field::kLength:
      return {40, 8, Flag::kNone};  // TODO impl kZeroEncodesMax
    case Field::kNesn:
      return {48, 8, Flag::kNone};
    case Field::kSn:
      return {56, 8, Flag::kNone};
    case Field::kPayload:
      return {64, kSessionPacketPayloadBytes * 8, Flag::kNone};
    }
    __builtin_trap();
  }

  uint8_t const* GetFieldPointer(Field f) const {
    switch (f) {
    case Field::kSessionId:
      return reinterpret_cast<uint8_t const*>(&(id));
    case Field::kType:
      return reinterpret_cast<uint8_t const*>(&(type));
    case Field::kNesn:
      return reinterpret_cast<uint8_t const*>(&(nesn));
    case Field::kSn:
      return reinterpret_cast<uint8_t const*>(&(sn));
    case Field::kLength:
      return reinterpret_cast<uint8_t const*>(&(length));
    case Field::kPayload:
      return reinterpret_cast<uint8_t const*>(&(payload));
    }
    __builtin_trap();
  }

  uint8_t* GetFieldPointer(Field f) {
    using ConstThis = const Packet<kType>*;
    // this is legal because we know that the original type is non-const
    return const_cast<uint8_t*>(const_cast<ConstThis>(this)->GetFieldPointer(f));
  }

  WireSessionId id;
  SubType type;
  WirePayloadLength length;
  SequenceNumber nesn;
  SequenceNumber sn;
  SessionPacketPayload payload;
};
using SessionPacket = Packet<PacketType::kSession>;
inline bool operator==(const Packet<PacketType::kSession> &lhs, const Packet<PacketType::kSession> &rhs) {
  return (lhs.id == rhs.id && lhs.type == rhs.type &&
          lhs.length == rhs.length && lhs.nesn == rhs.nesn &&
          lhs.sn == rhs.sn && lhs.payload == rhs.payload);
}

inline const char* TypeStr(Packet<PacketType::kSession>::SubType t) {
  using P = Packet<PacketType::kSession>;
  switch (t) {
  case P::kNack:
    return "<NACK>";
  case P::kData:
    return "<DATA>";
  case P::kConnectionRequest:
    return "<CNRQ>";
  case P::kConnectionAccept:
    return "<CNAC>";
  }
  assert(false && "Unknown Packet Type");
}

template <>
struct Packet<PacketType::kAdvertising> {
  static constexpr PacketType kType = PacketType::kAdvertising;

  enum class Field {
    kSourceAddress = 0,
  };
  static constexpr Field kFinalField = Field::kSourceAddress;

  static constexpr PacketFieldInfo FieldMetadata(Field f) {
    using Flag = PacketFieldFlags;
    switch (f) {
    case Field::kSourceAddress:
      return {0, 32, Flag::kNone};
    }
    __builtin_trap();
  }

  const uint8_t* GetFieldPointer(Field f) const {
    switch (f) {
    case Field::kSourceAddress:
      return reinterpret_cast<uint8_t const*>(&(source_address));
    }
    __builtin_trap();
  }

  uint8_t* GetFieldPointer(Field f) {
    using ConstThis = const Packet<kType>*;
    // this is legal because we know that the original type is non-const
    return const_cast<uint8_t*>(const_cast<ConstThis>(this)->GetFieldPointer(f));
  }

  WireAddress source_address;
};

using AdvertisingPacket = Packet<PacketType::kAdvertising>;
inline bool operator==(const Packet<PacketType::kAdvertising> &lhs, const Packet<PacketType::kAdvertising> &rhs) {
  return (lhs.source_address == rhs.source_address);
}

template <>
struct Packet<PacketType::kConnectionRequest> {
  static constexpr PacketType kType = PacketType::kConnectionRequest;

  enum class Field {
    kSourceAddress = 0,
    kTargetAddress,
  };
  static constexpr Field kFinalField = Field::kTargetAddress;

  static constexpr PacketFieldInfo FieldMetadata(Field f) {
    using Flag = PacketFieldFlags;
    switch (f) {
    case Field::kSourceAddress:
      return {0, 32, Flag::kNone};
    case Field::kTargetAddress:
      return {32, 32, Flag::kNone};
    }
    __builtin_trap();
  }

  const uint8_t* GetFieldPointer(Field f) const {
    switch (f) {
    case Field::kSourceAddress:
      return reinterpret_cast<uint8_t const*>(&(source_address));
    case Field::kTargetAddress:
      return reinterpret_cast<uint8_t const*>(&(target_address));
      break;
    }
    __builtin_trap();
  }

  uint8_t* GetFieldPointer(Field f) {
    using ConstThis = const Packet<kType>*;
    // this is legal because we know that the original type is non-const
    return const_cast<uint8_t*>(const_cast<ConstThis>(this)->GetFieldPointer(f));
  }

  WireAddress source_address;
  WireAddress target_address;
};

inline bool operator==(const Packet<PacketType::kConnectionRequest> &lhs, const Packet<PacketType::kConnectionRequest> &rhs) {
  return (lhs.source_address == rhs.source_address && lhs.target_address == rhs.target_address);
}

template <>
struct Packet<PacketType::kConnectionAccept> {
  static constexpr PacketType kType = PacketType::kConnectionAccept;

  enum class Field {
    kSourceAddress = 0,
    kTargetAddress,
    kSessionStartTime,
    kSessionId,
    // TODO specify and hop to a new frequency
  };
  static constexpr Field kFinalField = Field::kSessionId;

  static constexpr PacketFieldInfo FieldMetadata(Field f) {
    using Flag = PacketFieldFlags;
    switch (f) {
    case Field::kSourceAddress:
      return {0, 32, Flag::kNone};
    case Field::kTargetAddress:
      return {32, 32, Flag::kNone};
    case Field::kSessionStartTime:
      return {64, 8 * sizeof(WireTimePoint), Flag::kNone};
    case Field::kSessionId:
      return {128,  8 * sizeof(WireSessionId), Flag::kNone};
    }
    __builtin_trap();
  }

  const uint8_t* GetFieldPointer(Field f) const {
    switch (f) {
    case Field::kSourceAddress:
      return reinterpret_cast<uint8_t const*>(&(source_address));
    case Field::kTargetAddress:
      return reinterpret_cast<uint8_t const*>(&(target_address));
    case Field::kSessionStartTime:
      return reinterpret_cast<uint8_t const*>(&(session_start_time));
    case Field::kSessionId:
      return reinterpret_cast<uint8_t const*>(&(session_id));
    }
    __builtin_trap();
  }

  uint8_t* GetFieldPointer(Field f) {
    using ConstThis = const Packet<kType>*;
    // this is legal because we know that the original type is non-const
    return const_cast<uint8_t*>(const_cast<ConstThis>(this)->GetFieldPointer(f));
  }

  WireAddress source_address;
  WireAddress target_address;
  WireTimePoint session_start_time;  // TODO hold a deserialized TimePoint
  WireSessionId session_id;
};

inline bool operator==(const Packet<PacketType::kConnectionAccept> &lhs, const Packet<PacketType::kConnectionAccept> &rhs) {
  return (lhs.source_address == rhs.source_address &&
          lhs.target_address == rhs.target_address &&
          lhs.session_start_time == rhs.session_start_time &&
          lhs.session_id == rhs.session_id);
}

} // namespace lora_chat
