#include "packet.hpp"

#include "gtest/gtest.h"

namespace {

TEST(SerDe, Basic) {
  using Packet = lora_chat::Packet;
  Packet p1{};
  EXPECT_EQ(p1, Packet::Deserialize(p1.Serialize()));

  p1.sn = lora_chat::SequenceNumber(1);
  p1.length = 2;
  EXPECT_EQ(p1, Packet::Deserialize(p1.Serialize()));
}

TEST(SerDe, Chained) {
  using Packet = lora_chat::Packet;
  using WirePacket = lora_chat::WirePacket;
  using Sn = lora_chat::SequenceNumber;

  Packet p1{
      .id = 0xAAAAAAAA,
      .type = Packet::kNack,
      .length = 0xDD,
      .nesn = Sn(0xBB),
      .sn = Sn(0xCC),
      .payload{0xFF},
  };
  const WirePacket p1_wire = p1.Serialize();

  Packet p2{Packet::Deserialize(p1_wire)};
  const WirePacket p2_wire = p2.Serialize();

  Packet p3{};
  p3.DeserializeInto(p2_wire);

  using Field = lora_chat::PacketField;
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

} // namespace
