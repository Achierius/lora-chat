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

#include "radio_interface.hpp"
#include "test_utils.hpp"
#include "gtest/gtest.h"

namespace {

using namespace lora_chat::testutils;

TEST(ActionOrdering, SimpleFollower) {
  using Session = lora_chat::Session;
  using AgentAction = lora_chat::AgentAction;

  constexpr auto kTransmitTime = std::chrono::milliseconds(10);
  constexpr auto kGapTime = std::chrono::milliseconds(10);

  CountingRadio radio{};
  lora_chat::MessagePipe pipe{};
  Session session{std::chrono::steady_clock::now(), 0, kTransmitTime, kGapTime,
                  false};

  // this is the NEXT action the session will take
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
            AgentAction::kTransmitNextMessage)
      << "Receive (Period 0)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive)
      << "Transmit (Period 0)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
            AgentAction::kRetransmitMessage)
      << "Receive (Period 1)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive)
      << "Transmit (Period 1)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
            AgentAction::kRetransmitMessage)
      << "Receive (Period 2)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive)
      << "Transmit (Period 2)";
}

TEST(ActionOrdering, GaplessFollower) {
  using Session = lora_chat::Session;
  using AgentAction = lora_chat::AgentAction;

  constexpr auto kTransmitTime = std::chrono::milliseconds(10);
  constexpr auto kGapTime = std::chrono::milliseconds(0);

  CountingRadio radio{};
  lora_chat::MessagePipe pipe{};
  Session session{std::chrono::steady_clock::now(), 0, kTransmitTime, kGapTime,
                  false};

  // this is the NEXT action the session will take
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
            AgentAction::kTransmitNextMessage)
      << "Receive (Period 0)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive)
      << "Transmit (Period 0)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
            AgentAction::kRetransmitMessage)
      << "Receive (Period 1)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive)
      << "Transmit (Period 1)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
            AgentAction::kRetransmitMessage)
      << "Receive (Period 2)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive)
      << "Transmit (Period 2)";
}

TEST(ActionOrdering, SimpleInitiator) {
  using Session = lora_chat::Session;
  using AgentAction = lora_chat::AgentAction;

  constexpr auto kTransmitTime = std::chrono::milliseconds(10);
  constexpr auto kGapTime = std::chrono::milliseconds(10);

  CountingRadio radio{};
  lora_chat::MessagePipe pipe{};
  Session session{lora_chat::Now(), 0, kTransmitTime, kGapTime, true};

  // this is the NEXT action the session will take
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive)
      << "Transmit (Period 0)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
            AgentAction::kRetransmitMessage)
      << "Receive (Period 0)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive)
      << "Transmit (Period 1)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
            AgentAction::kRetransmitMessage)
      << "Receive (Period 1)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive)
      << "Transmit (Period 2)";
  EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
            AgentAction::kRetransmitMessage)
      << "Receive (Period 2)";
}

TEST(ActionTimings, VariousTimes) {
  using Session = lora_chat::Session;
  using Duration = lora_chat::Duration;
  using AgentAction = lora_chat::AgentAction;
  using TimePoint = lora_chat::TimePoint;

  constexpr std::array<std::pair<Duration, Duration>, 6> kTestConfigs{{
      {std::chrono::milliseconds(10), std::chrono::milliseconds(10)},
      {std::chrono::milliseconds(20), std::chrono::milliseconds(5)},
      {std::chrono::milliseconds(5), std::chrono::milliseconds(20)},
      {std::chrono::milliseconds(15), std::chrono::milliseconds(0)},
      {std::chrono::milliseconds(5), std::chrono::milliseconds(5)},
      {std::chrono::milliseconds(3), std::chrono::milliseconds(30)},
  }};
  constexpr std::array<std::pair<Duration, Duration>, 3> kSensitiveTestConfigs{{
      {std::chrono::milliseconds(2), std::chrono::milliseconds(5)},
      {std::chrono::milliseconds(1), std::chrono::milliseconds(1)},
      {std::chrono::milliseconds(1), std::chrono::microseconds(10)},
  }};
  constexpr int kPeriodsPerConfig{10};

  auto test_as_initiator = [&](Duration transmit, Duration gap,
                               TimePoint start_time) {
    CountingRadio radio{};
    lora_chat::MessagePipe pipe{};
    Session session{start_time, 0, transmit, gap, true};
    session.SleepUntilStartTime();
    for (int i = 0; i < kPeriodsPerConfig; i++) {
      // this is the NEXT action the session will take
      EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
                AgentAction::kReceive)
          << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{1, 0}))
          << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
                AgentAction::kRetransmitMessage)
          << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 1}))
          << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
    }
  };
  auto test_as_follower = [&](Duration transmit, Duration gap,
                              TimePoint start_time) {
    CountingRadio radio{};
    lora_chat::MessagePipe pipe{};
    Session session{start_time, 0, transmit, gap, false};
    session.SleepUntilStartTime();
    for (int i = 0; i < kPeriodsPerConfig; i++) {
      // this is the NEXT action the session will take
      EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
                !i ? AgentAction::kTransmitNextMessage
                   : AgentAction::kRetransmitMessage)
          << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 1}))
          << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
                AgentAction::kReceive)
          << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{1, 0}))
          << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
    }
  };

  std::vector<std::thread> executors{};
  for (auto const &[transmit, gap] : kTestConfigs) {
    const auto start_time = lora_chat::Now() + std::chrono::milliseconds(20);
    executors.push_back(
        std::thread(test_as_initiator, transmit, gap, start_time));
    executors.push_back(
        std::thread(test_as_follower, transmit, gap, start_time));
  }
  for (auto &thread : executors)
    thread.join();

  for (auto const &[transmit, gap] : kSensitiveTestConfigs) {
    const auto start_time = lora_chat::Now() + std::chrono::milliseconds(20);
    std::thread thr_initiator(test_as_initiator, transmit, gap, start_time);
    std::thread thr_follower(test_as_follower, transmit, gap, start_time);
    thr_initiator.join();
    thr_follower.join();
  }
}

TEST(ActionTimings, VerySmallDuration) {
  using Session = lora_chat::Session;
  using AgentAction = lora_chat::AgentAction;

  CountingRadio radio{};
  lora_chat::MessagePipe pipe{};
  auto start_time = lora_chat::Now() + std::chrono::milliseconds(50);
  Session session{start_time, 0, std::chrono::microseconds(250),
                  std::chrono::microseconds(100), false};
  session.SleepUntilStartTime();
  for (int i = 0; i < 20; i++) {
    // this is the NEXT action the session will take
    EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe),
              !i ? AgentAction::kTransmitNextMessage
                 : AgentAction::kRetransmitMessage)
        << "(A) " << i;
    EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 1}))
        << "(A) " << i;
    EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive)
        << "(B) " << i;
    EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{1, 0}))
        << "(B) " << i;
  }
}

constexpr static TextTag kPingTag = {"PING"};
constexpr static TextTag kPongTag = {"PONG"};
constexpr static TextTag kPingerTag = {"Pinger"};
constexpr static TextTag kPongerTag = {"Ponger"};

TEST(PingPong, Simple) {
  using MessagePipe = lora_chat::MessagePipe;
  using AgentAction = lora_chat::AgentAction;
  using Session = lora_chat::Session;

  MessagePipe ping_pipe{MakeMessage<kPingTag>, ConsumeMessage<kPingerTag>};
  MessagePipe pong_pipe{MakeMessage<kPongTag>, ConsumeMessage<kPongerTag>};

  LocalRadio radio(std::chrono::milliseconds(8));

  constexpr int kPeriods{4};
  constexpr auto kTransmitTime = std::chrono::milliseconds(10);
  constexpr auto kGapTime = std::chrono::milliseconds(5);

  auto start_time = lora_chat::Now() + std::chrono::milliseconds(10);
  Session ponger(start_time, 0, kTransmitTime, kGapTime, false);
  Session pinger(start_time, 1, kTransmitTime, kGapTime, true);

  std::thread ponger_thread([&start_time, &ponger, &radio, &pong_pipe]() {
    std::this_thread::sleep_until(start_time);
    for (int i = 0; i < kPeriods; i++) {
      EXPECT_EQ(ponger.ExecuteCurrentAction(radio, pong_pipe),
                AgentAction::kTransmitNextMessage)
          << " (A) -> ponger @ " << i;
      EXPECT_EQ(ponger.ExecuteCurrentAction(radio, pong_pipe),
                AgentAction::kReceive)
          << " (B) ponger @ " << i;
    }
  });

  std::this_thread::sleep_until(start_time);
  for (int i = 0; i < kPeriods; i++) {
    EXPECT_EQ(pinger.ExecuteCurrentAction(radio, ping_pipe),
              AgentAction::kReceive)
        << " (A) pinger @ " << i;
    EXPECT_EQ(pinger.ExecuteCurrentAction(radio, ping_pipe),
              AgentAction::kTransmitNextMessage)
        << "  (B)pinger @ " << i;
  }

  ponger_thread.join();
}

TEST(PingPong, OneSidedFailures) {
  using MessagePipe = lora_chat::MessagePipe;
  using AgentAction = lora_chat::AgentAction;
  using Session = lora_chat::Session;

  MessagePipe ping_pipe{MakeMessage<kPingTag>, ConsumeMessage<kPingerTag>};
  MessagePipe pong_pipe{MakeMessage<kPongTag>, ConsumeMessage<kPongerTag>};

  FallibleLocalRadio radio(std::chrono::milliseconds(8), 4, 0);

  constexpr int kPeriods{8};
  constexpr auto kTransmitTime = std::chrono::milliseconds(10);
  constexpr auto kGapTime = std::chrono::milliseconds(5);

  auto start_time = lora_chat::Now() + std::chrono::milliseconds(10);
  Session ponger(start_time, 0, kTransmitTime, kGapTime, false);
  Session pinger(start_time, 1, kTransmitTime, kGapTime, true);

  std::thread ponger_thread([&start_time, &ponger, &radio, &pong_pipe]() {
    std::this_thread::sleep_until(start_time);
    for (int i = 0; i < kPeriods; i++) {
      // The first message to drop will be the second one we send, so one after
      // that and every other transmission thereafter we will retransmit
      AgentAction transmitAction = AgentAction::kTransmitNextMessage;
      if (i > 1 && ((i + 1) % 2))
        transmitAction = AgentAction::kRetransmitMessage;
      EXPECT_EQ(ponger.ExecuteCurrentAction(radio, pong_pipe), transmitAction)
          << " (A) -> ponger @ " << i;
      EXPECT_EQ(ponger.ExecuteCurrentAction(radio, pong_pipe),
                AgentAction::kReceive)
          << " (B) ponger @ " << i;
    }
  });

  std::this_thread::sleep_until(start_time);
  for (int i = 0; i < kPeriods; i++) {
    const AgentAction transmitAction = ((i + 1) % 2)
                                           ? AgentAction::kTransmitNextMessage
                                           : AgentAction::kTransmitNack;
    EXPECT_EQ(pinger.ExecuteCurrentAction(radio, ping_pipe),
              AgentAction::kReceive)
        << " (A) pinger @ " << i;
    EXPECT_EQ(pinger.ExecuteCurrentAction(radio, ping_pipe), transmitAction)
        << "  (B)pinger @ " << i;
  }

  ponger_thread.join();
}

} // namespace
