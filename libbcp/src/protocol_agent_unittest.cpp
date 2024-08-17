#include "protocol_agent.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <semaphore>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "radio_interface.hpp"
#include "session.hpp"
#include "test_utils.hpp"
#include "gtest/gtest.h"

namespace {

using namespace lora_chat::testutils;

TEST(ActionOrdering, DontActUntilToldTo) {
  using ProtocolAgent = lora_chat::ProtocolAgent;

  CountingRadio radio{};
  lora_chat::MessagePipe pipe{};
  ProtocolAgent agent {0, radio, pipe};

  EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 0}));
  agent.SetGoal(ProtocolAgent::ConnectionGoal::kSeekConnection);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 0}));

  agent.SetGoal(ProtocolAgent::ConnectionGoal::kAdvertiseConnection);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 0}));
}

TEST(ActionOrdering, AdvertiseNoResponse) {
  using ProtocolAgent = lora_chat::ProtocolAgent;
  using Goal = ProtocolAgent::ConnectionGoal;

  CountingRadio radio{{true, false}, std::chrono::milliseconds(10)};
  lora_chat::MessagePipe pipe{};
  ProtocolAgent agent {0, radio, pipe};

  EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 0}));
  agent.SetGoal(Goal::kAdvertiseConnection);
  for (int i = 0; i < 3; i++) {
    // This will first transmit an advertisement packet.
    // Then it  will continue attempting to receive until it gets a conn-req
    // packet. As such we expect to see the device try to receive more than
    // once.
    agent.ExecuteAgentAction();
    auto [trans, recv] = radio.GetAndClearObservedActions();
    EXPECT_EQ(trans, 1);
    EXPECT_GE(recv, 2);
  }
}

TEST(ActionOrdering, SeekNoResponse) {
  using ProtocolAgent = lora_chat::ProtocolAgent;
  using Goal = ProtocolAgent::ConnectionGoal;

  CountingRadio radio{std::chrono::milliseconds(10)};
  lora_chat::MessagePipe pipe{};
  ProtocolAgent agent {0, radio, pipe};

  EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 0}));
  agent.SetGoal(Goal::kSeekConnection);
  for (int i = 0; i < 3; i++) {
    agent.ExecuteAgentAction();
    EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 1}));
  }
}

TEST(ActionOrdering, AdvertiseAndSeek) {
  using ProtocolAgent = lora_chat::ProtocolAgent;
  using Goal = ProtocolAgent::ConnectionGoal;

  // Will not receive anything
  CountingRadio radio{{true, false}, std::chrono::milliseconds(10)};
  lora_chat::MessagePipe pipe{};
  ProtocolAgent agent {0, radio, pipe};

  EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 0}));
  agent.SetGoal(Goal::kSeekAndAdvertiseConnection);
  for (int i = 0; i < 3; i++) {
    // Advertise
    agent.ExecuteAgentAction();
    auto [trans, recv] = radio.GetAndClearObservedActions();
    EXPECT_EQ(trans, 1);
    EXPECT_GE(recv, 2);

    // Seek
    agent.ExecuteAgentAction();
  }
}


TEST(ActionOrdering, AdvertiseSuccess) {
  using ProtocolAgent = lora_chat::ProtocolAgent;
  using Packet = lora_chat::Packet;
  using Status = lora_chat::RadioInterface::Status;
  using Goal = ProtocolAgent::ConnectionGoal;

  auto send_conreq = [](std::span<uint8_t> out) {
    Packet p{};
    p.type = Packet::kConnectionRequest;
    p.id = 3;
    auto w_p = p.Serialize();
    assert(out.size_bytes() >= w_p.size());
    std::copy(w_p.begin(), w_p.end(), out.begin());
    return Status::kSuccess;
  };
  CountingRadio radio{true, send_conreq, std::chrono::milliseconds(50)};
  lora_chat::MessagePipe pipe{};
  ProtocolAgent agent {0, radio, pipe};

  EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 0}));
  agent.SetGoal(Goal::kAdvertiseConnection);
  // First we send an advert, and then receive a conreq right away
  agent.ExecuteAgentAction();
  EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{1, 1}));
  // Then we accept the request
  agent.ExecuteAgentAction();
  EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{1, 0}));
}

TEST(ActionOrdering, SeekSuccess) {
  using ProtocolAgent = lora_chat::ProtocolAgent;
  using Packet = lora_chat::Packet;
  using Status = lora_chat::RadioInterface::Status;
  using Goal = ProtocolAgent::ConnectionGoal;

  auto send_advert = [](std::span<uint8_t> out) {
    Packet p{};
    p.type = Packet::kAdvertisement;
    p.id = 3;
    auto w_p = p.Serialize();
    assert(out.size_bytes() >= w_p.size());
    std::copy(w_p.begin(), w_p.end(), out.begin());
    return Status::kSuccess;
  };
  CountingRadio radio{true, send_advert, std::chrono::milliseconds(50)};
  lora_chat::MessagePipe pipe{};
  ProtocolAgent agent {0, radio, pipe};

  EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 0}));
  agent.SetGoal(Goal::kSeekConnection);
  // First we receive an advertisement
  agent.ExecuteAgentAction();
  EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 1}));
  // Then we respond with a connection request, and thereafter receive messages
  // to see if they accepted
  agent.ExecuteAgentAction();
  auto [trans, recv] = radio.GetAndClearObservedActions();
  EXPECT_EQ(trans, 1);
  EXPECT_GE(recv, 2);
}

TEST(Handshake, Simple) {
  using ProtocolAgent = lora_chat::ProtocolAgent;
  using Goal = ProtocolAgent::ConnectionGoal;

  LocalRadio radio{std::chrono::milliseconds(50)};
  lora_chat::MessagePipe pipe_a{};
  lora_chat::MessagePipe pipe_b{};
  ProtocolAgent agent_a {0, radio, pipe_a};
  ProtocolAgent agent_b {1, radio, pipe_b};

  agent_a.SetGoal(Goal::kAdvertiseConnection);
  agent_b.SetGoal(Goal::kSeekConnection);

  // TODO actually test the handshake
}

} // namespace
