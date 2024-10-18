#include "packet.hpp"
#include "wire_packet.hpp"

#include "gtest/gtest.h"

#include <sstream>

namespace {

template <lora_chat::PacketType Pt>
bool SimulateRadioTransmission(lora_chat::ReceiveBuffer &dest,
                               lora_chat::NewWirePacket<Pt> const &src) {
  if (src.size() > dest.size()) return false;

  std::memcpy(dest.data(), src.data(), src.size());
  return true;
}

TEST(SerDe, Basic) {
  using PType = lora_chat::PacketType;
  using Packet = lora_chat::Packet<PType::kSession>;
  using ReceiveBuffer = lora_chat::ReceiveBuffer;
  using lora_chat::Deserialize;
  using lora_chat::Serialize;

  Packet p1{};
  auto wp_1 = Serialize(p1);
  ReceiveBuffer buff_1{};
  ASSERT_TRUE(SimulateRadioTransmission<PType::kSession>(buff_1, wp_1));
  std::memcpy(buff_1.data(), wp_1.data(), wp_1.size());
  auto wp_1_deser = Deserialize<PType::kSession>(buff_1);
  ASSERT_TRUE(wp_1_deser.has_value());

  p1.sn = lora_chat::SequenceNumber(1);
  p1.length = 2;
  auto wp_2 = Serialize(p1);
  ReceiveBuffer buff_2{};
  ASSERT_TRUE(SimulateRadioTransmission<PType::kSession>(buff_2, wp_2));
  auto wp_2_deser = Deserialize<PType::kSession>(buff_2);
  ASSERT_TRUE(wp_2_deser.has_value());
  EXPECT_EQ(p1, *wp_2_deser);
}

TEST(SerDe, Chained) {
  using PType = lora_chat::PacketType;
  using Packet = lora_chat::Packet<PType::kSession>;
  using lora_chat::Deserialize;
  using lora_chat::Serialize;
  using WirePacket = lora_chat::NewWirePacket<PType::kSession>;
  using Sn = lora_chat::SequenceNumber;

  lora_chat::ReceiveBuffer recv_buff{};

  Packet p1{
      .id = 0xAAAAAAAA,
      .type = Packet::kNack,
      .length = 0xDD,
      .nesn = Sn(0xBB),
      .sn = Sn(0xCC),
      .payload{0xFF},
  };
  const WirePacket p1_wire = Serialize(p1);
  ASSERT_TRUE(SimulateRadioTransmission<PType::kSession>(recv_buff, p1_wire));

  auto maybe_p2 = Deserialize<PType::kSession>(recv_buff);
  ASSERT_TRUE(maybe_p2.has_value());
  Packet p2(*maybe_p2);
  const WirePacket p2_wire = Serialize(p2);
  ASSERT_TRUE(SimulateRadioTransmission<PType::kSession>(recv_buff, p2_wire));

  auto maybe_p3 = Deserialize<PType::kSession>(recv_buff);
  ASSERT_TRUE(maybe_p3.has_value());
  Packet p3(*maybe_p3);

  using Field = Packet::Field;
  for (int i = 0; i <= static_cast<int>(Field::kPayload); i++) {
    // Not super useful but it does make sure we catch any new additions to the
    // PacketField enum, if nothing else
    Field f = static_cast<Field>(i);
    switch (f) {
    case Field::kSessionId:
      EXPECT_EQ(p1.id, p2.id);
      EXPECT_EQ(p1.id, p3.id);
      break;
    case Field::kNesn:
      EXPECT_EQ(p1.nesn, p2.nesn);
      EXPECT_EQ(p1.nesn, p3.nesn);
      break;
    case Field::kSn:
      EXPECT_EQ(p1.sn, p2.sn);
      EXPECT_EQ(p1.sn, p3.sn);
      break;
    case Field::kLength:
      EXPECT_EQ(p1.length, p2.length);
      EXPECT_EQ(p1.length, p3.length);
      break;
    case Field::kPayload:
      EXPECT_EQ(p1.payload, p2.payload);
      EXPECT_EQ(p1.payload, p3.payload);
      break;
    case Field::kType:
      EXPECT_EQ(p1.type, p2.type);
      EXPECT_EQ(p1.type, p2.type);
      break;
    }
  }

  EXPECT_EQ(p1, p2);
  EXPECT_EQ(p1, p3);
  EXPECT_EQ(p2, p3);

  p1.nesn = Sn(0xCC);
  EXPECT_NE(p1, p2);
  EXPECT_NE(p1, p3);
  EXPECT_EQ(p2, p3);
}

TEST(SerDe, FailOnBadTag) {
  using PType = lora_chat::PacketType;
  using Packet = lora_chat::Packet<PType::kSession>;
  using lora_chat::Deserialize;
  using lora_chat::Serialize;
  using WirePacket = lora_chat::NewWirePacket<PType::kSession>;
  using Sn = lora_chat::SequenceNumber;

  Packet packet{
      .id = 0xAAAAAAAA,
      .type = Packet::kNack,
      .length = 0xDD,
      .nesn = Sn(0xBB),
      .sn = Sn(0xCC),
      .payload{0xFF},
  };
  WirePacket wire_packet = Serialize(packet);
  lora_chat::ReceiveBuffer recv_buff{};
  ASSERT_TRUE(SimulateRadioTransmission<PType::kSession>(recv_buff, wire_packet));
  EXPECT_TRUE(Deserialize<PType::kSession>(recv_buff).has_value());

  // Now muck up the tag and make sure it doesn't still deserialize
  static_assert(lora_chat::kWirePacketTagBits == 8);
  char bad_tag = static_cast<char>(PType::kSession) + 1;
  std::memset(recv_buff.data(), bad_tag, 1);
  EXPECT_FALSE(Deserialize<PType::kSession>(recv_buff).has_value());
}

} // namespace
