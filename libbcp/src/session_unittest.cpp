#include "packet.hpp"
#include "session.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <limits>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "radio_interface.hpp"
#include "test_utils.hpp"

namespace {

using namespace lora_chat::testutils;

TEST(ActionTimings, SimpleFollower) {
  using Session = lora_chat::Session;
  using AgentAction = lora_chat::AgentAction;

  constexpr auto kTransmitTime = std::chrono::milliseconds(10);
  constexpr auto kGapTime = std::chrono::milliseconds(10);

  CountingRadio radio{};
  lora_chat::MessagePipe pipe{};
  Session session {std::chrono::steady_clock::now(), 0, kTransmitTime, kGapTime};

  // this is the NEXT action the session will take
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kTransmitNextMessage) << "Receive (Period 0)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << "Transmit (Period 0)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kRetransmitMessage) << "Receive (Period 1)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << "Transmit (Period 1)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kRetransmitMessage) << "Receive (Period 2)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << "Transmit (Period 2)";
}

TEST(ActionTimings, GaplessFollower) {
  using Session = lora_chat::Session;
  using AgentAction = lora_chat::AgentAction;

  constexpr auto kTransmitTime = std::chrono::milliseconds(10);
  constexpr auto kGapTime = std::chrono::milliseconds(0);

  CountingRadio radio{};
  lora_chat::MessagePipe pipe{};
  Session session {std::chrono::steady_clock::now(), 0, kTransmitTime, kGapTime};

  // this is the NEXT action the session will take
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kTransmitNextMessage) << "Receive (Period 0)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << "Transmit (Period 0)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kRetransmitMessage) << "Receive (Period 1)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << "Transmit (Period 1)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kRetransmitMessage) << "Receive (Period 2)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << "Transmit (Period 2)";
}

TEST(ActionTimings, SimpleInitiator) {
  using Session = lora_chat::Session;
  using AgentAction = lora_chat::AgentAction;

  constexpr auto kTransmitTime = std::chrono::milliseconds(10);
  constexpr auto kGapTime = std::chrono::milliseconds(10);

  CountingRadio radio{};
  lora_chat::MessagePipe pipe{};
  Session session {0, kTransmitTime, kGapTime};
  std::this_thread::sleep_for(Session::kHandshakeLeadTime);

  // this is the NEXT action the session will take
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << "Transmit (Period 0)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kRetransmitMessage) << "Receive (Period 0)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << "Transmit (Period 1)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kRetransmitMessage) << "Receive (Period 1)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << "Transmit (Period 2)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kRetransmitMessage) << "Receive (Period 2)";
}

TEST(ActionTimings, VariousTimes) {
  using Session = lora_chat::Session;
  using Duration = lora_chat::Duration;
  using AgentAction = lora_chat::AgentAction;

  constexpr std::array<std::pair<Duration, Duration>, 9> kTestConfigs {{
    {std::chrono::milliseconds(10), std::chrono::milliseconds(10)},
    {std::chrono::milliseconds(20), std::chrono::milliseconds(5)},
    {std::chrono::milliseconds(5), std::chrono::milliseconds(20)},
    {std::chrono::milliseconds(15), std::chrono::milliseconds(0)},
    {std::chrono::milliseconds(5), std::chrono::milliseconds(5)},
    {std::chrono::milliseconds(2), std::chrono::milliseconds(5)},
    {std::chrono::milliseconds(1), std::chrono::milliseconds(1)},
    {std::chrono::milliseconds(1), std::chrono::milliseconds(40)},
    {std::chrono::milliseconds(1), std::chrono::microseconds(10)},
  }};
  constexpr int kPeriodsPerConfig {10}; 

  auto test_as_initiator = [&](Duration transmit, Duration gap) {
    CountingRadio radio{};
    lora_chat::MessagePipe pipe{};
    Session session {0, transmit, gap};
    std::this_thread::sleep_for(Session::kHandshakeLeadTime);
    for (int i = 0; i < kPeriodsPerConfig; i++) {
      // this is the NEXT action the session will take
      EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{1, 0})) << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kRetransmitMessage) << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 1})) << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
    }
  };
  auto test_as_follower = [&](Duration transmit, Duration gap) {
    CountingRadio radio{};
    lora_chat::MessagePipe pipe{};
    Session session {std::chrono::steady_clock::now(), 0, transmit, gap};
    for (int i = 0; i < kPeriodsPerConfig; i++) {
      // this is the NEXT action the session will take
      EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), !i ? AgentAction::kTransmitNextMessage : AgentAction::kRetransmitMessage) << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 1})) << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{1, 0})) << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
    }
  };

  std::vector<std::thread> executors {};
  for (auto const& [transmit, gap] : kTestConfigs) {
    executors.push_back(std::thread(test_as_initiator, transmit, gap));
    executors.push_back(std::thread(test_as_follower, transmit, gap));
  }
  for (auto& thread : executors) thread.join();
}

TEST(ActionTimings, VerySmallDuration) {
  using Session = lora_chat::Session;
  using AgentAction = lora_chat::AgentAction;

  CountingRadio radio{};
  lora_chat::MessagePipe pipe{};
  Session session {std::chrono::steady_clock::now(), 0, std::chrono::microseconds(250), std::chrono::microseconds(100)};
  for (int i = 0; i < 20; i++) {
    // this is the NEXT action the session will take
    EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), !i ? AgentAction::kTransmitNextMessage : AgentAction::kRetransmitMessage) << "(A) " << i;
    EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 1})) << "(A) " << i;
    EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << "(B) " << i;
    EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{1, 0})) << "(B) " << i;
  }
}

constexpr static TextTag kPingTag = { "PING" };
constexpr static TextTag kPongTag = { "PONG" };
constexpr static TextTag kPingerTag = { "Pinger" };
constexpr static TextTag kPongerTag = { "Ponger" };

TEST(PingPong, Simple) {
  using MessagePipe = lora_chat::MessagePipe;
  using AgentAction = lora_chat::AgentAction;
  using Session = lora_chat::Session;

  MessagePipe ping_pipe {MakeMessage<kPingTag>, ConsumeMessage<kPingerTag>};
  MessagePipe pong_pipe {MakeMessage<kPongTag>, ConsumeMessage<kPongerTag>};

  LocalRadio radio(std::chrono::milliseconds(8));

  constexpr int kPeriods {4};
  constexpr auto kTransmitTime = std::chrono::milliseconds(10);
  constexpr auto kGapTime = std::chrono::milliseconds(5);

  Session ponger(std::chrono::steady_clock::now() + Session::kHandshakeLeadTime, 0, kTransmitTime, kGapTime);
  Session pinger(0, kTransmitTime, kGapTime);
  std::this_thread::sleep_for(Session::kHandshakeLeadTime);

  std::thread ponger_thread([&ponger, &radio, &pong_pipe]() {
    for (int i = 0; i < kPeriods; i++) {
      EXPECT_EQ(ponger.ExecuteCurrentAction(radio, pong_pipe), AgentAction::kTransmitNextMessage) << " (A) -> ponger @ " << i;
      EXPECT_EQ(ponger.ExecuteCurrentAction(radio, pong_pipe), AgentAction::kReceive) << " (B) ponger @ " << i;
    }
  });

  for (int i = 0; i < kPeriods; i++) {
    EXPECT_EQ(pinger.ExecuteCurrentAction(radio, ping_pipe), AgentAction::kReceive) << " (A) pinger @ " << i;
    EXPECT_EQ(pinger.ExecuteCurrentAction(radio, ping_pipe), AgentAction::kTransmitNextMessage) << "  (B)pinger @ " << i;
  }

  ponger_thread.join();
}

TEST(PingPong, OneSidedFailures) {
  using MessagePipe = lora_chat::MessagePipe;
  using AgentAction = lora_chat::AgentAction;
  using Session = lora_chat::Session;

  MessagePipe ping_pipe {MakeMessage<kPingTag>, ConsumeMessage<kPingerTag>};
  MessagePipe pong_pipe {MakeMessage<kPongTag>, ConsumeMessage<kPongerTag>};

  FallibleLocalRadio radio(std::chrono::milliseconds(8), 4, 0);

  constexpr int kPeriods {8};
  constexpr auto kTransmitTime = std::chrono::milliseconds(10);
  constexpr auto kGapTime = std::chrono::milliseconds(5);

  Session ponger(std::chrono::steady_clock::now() + Session::kHandshakeLeadTime, 0, kTransmitTime, kGapTime);
  Session pinger(0, kTransmitTime, kGapTime);
  std::this_thread::sleep_for(Session::kHandshakeLeadTime);

  std::thread ponger_thread([&ponger, &radio, &pong_pipe]() {
    for (int i = 0; i < kPeriods; i++) {
      // The first message to drop will be the second one we send, so one after
      // that and every other transmission thereafter we will retransmit
      AgentAction transmitAction = AgentAction::kTransmitNextMessage;
      if (i > 1 && ((i + 1) % 2)) transmitAction = AgentAction::kRetransmitMessage;
      EXPECT_EQ(ponger.ExecuteCurrentAction(radio, pong_pipe), transmitAction) << " (A) -> ponger @ " << i;
      EXPECT_EQ(ponger.ExecuteCurrentAction(radio, pong_pipe), AgentAction::kReceive) << " (B) ponger @ " << i;
    }
  });

  for (int i = 0; i < kPeriods; i++) {
    const AgentAction transmitAction = ((i + 1) % 2) ? AgentAction::kTransmitNextMessage : AgentAction::kTransmitNack;
    EXPECT_EQ(pinger.ExecuteCurrentAction(radio, ping_pipe), AgentAction::kReceive) << " (A) pinger @ " << i;
    EXPECT_EQ(pinger.ExecuteCurrentAction(radio, ping_pipe), transmitAction) << "  (B)pinger @ " << i;
  }

  ponger_thread.join();
}

} // namespace
