#include "packet.hpp"
#include "session.hpp"

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

#include "gtest/gtest.h"
#include "radio_interface.hpp"

namespace {

class CountingRadio : public lora_chat::RadioInterface {
public:
  Status Transmit(std::span<uint8_t const> buffer) {
    observed_actions_.first++;
    return Status::kSuccess;
  }

  Status Receive(std::span<uint8_t> buffer_out) {
    observed_actions_.second++;
    return Status::kSuccess;
  }

  size_t MaximumMessageLength() const { return 1 << 10; }

  std::pair<int, int> GetAndClearObservedActions() {
    auto ret = observed_actions_;
    observed_actions_ = {0, 0};
    return ret;
  }
private:
  std::pair<int, int> observed_actions_ {0, 0};
};

class LocalRadio : public lora_chat::RadioInterface {
public:
  // TODO this is a bit of a cludge, prefer a proper barrier w/ timeout
  LocalRadio(std::chrono::milliseconds timeout)
    : timeout_(timeout) {}

  Status Transmit(std::span<uint8_t const> buffer) {
    if (buffer.size() > MaximumMessageLength()) return Status::kBadBufferSize;
    std::scoped_lock lock(transmission_lock_);
    {
      std::scoped_lock lock(buffer_lock_);
      inflight_buffer_ = buffer;
    }
    transmission_ready_.release();
    std::this_thread::sleep_for(timeout_);
    transmission_ready_.try_acquire();
    return Status::kSuccess;
  }

  Status Receive(std::span<uint8_t> buffer_out) {
    if (!transmission_ready_.try_acquire_for(timeout_)) return Status::kTimeout;
    if (buffer_out.size() < inflight_buffer_.size()) return Status::kBadBufferSize;
    {
      std::scoped_lock lock(buffer_lock_);
      std::copy(inflight_buffer_.begin(), inflight_buffer_.end(), buffer_out.begin());
    }
    return Status::kSuccess;
  }

  size_t MaximumMessageLength() const { return 1 << 10; }

private:
  std::mutex transmission_lock_ {};
  std::binary_semaphore transmission_ready_ {0};
  std::mutex buffer_lock_ {};
  std::span<uint8_t const> inflight_buffer_ {};
  std::chrono::milliseconds timeout_;
};

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
  using Duration = Session::Duration;
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

TEST(TwoWayRadio, Simple) {
  using WirePacketPayload = lora_chat::WirePacketPayload;
  using MessagePipe = lora_chat::MessagePipe;
  using AgentAction = lora_chat::AgentAction;
  using Session = lora_chat::Session;

  constexpr bool kLogReceivedMessages = false;
  LocalRadio radio(std::chrono::milliseconds(8));
  auto ping_log_msg = []([[maybe_unused]] WirePacketPayload &&msg) {
    if constexpr (kLogReceivedMessages)
      printf("pinger received message: \"%s\"\n", reinterpret_cast<const char*>(&msg));
  };
  auto pong_log_msg = []([[maybe_unused]] WirePacketPayload &&msg) {
    if constexpr (kLogReceivedMessages)
      printf("ponger received message: \"%s\"\n", reinterpret_cast<const char*>(&msg));
  };

  auto ping_fn = []() {
    WirePacketPayload p {0};
    const char* ping = "ping";
    std::strcpy(reinterpret_cast<char*>(&p), ping);

    return std::optional<WirePacketPayload>(p);
  };
  MessagePipe ping_pipe {ping_fn, ping_log_msg};

  auto pong_fn = []() {
    WirePacketPayload p {0};
    const char* ping = "pong";
    std::strcpy(reinterpret_cast<char*>(&p), ping);

    return std::optional<WirePacketPayload>(p);
  };
  MessagePipe pong_pipe {pong_fn, pong_log_msg};

  constexpr int kPeriodsPerConfig {4};
  constexpr auto kTransmitTime = std::chrono::milliseconds(10);
  constexpr auto kGapTime = std::chrono::milliseconds(5);

  Session ponger(std::chrono::steady_clock::now() + Session::kHandshakeLeadTime, 0, kTransmitTime, kGapTime);
  Session pinger(0, kTransmitTime, kGapTime);
  std::this_thread::sleep_for(Session::kHandshakeLeadTime);

  std::thread ponger_thread([&ponger, &radio, &pong_pipe]() {
    for (int i = 0; i < kPeriodsPerConfig; i++) {
      EXPECT_EQ(ponger.ExecuteCurrentAction(radio, pong_pipe), AgentAction::kTransmitNextMessage) << " (A) -> ponger @ " << i;
      EXPECT_EQ(ponger.ExecuteCurrentAction(radio, pong_pipe), AgentAction::kReceive) << " (B) ponger @ " << i;
    }
  });

  for (int i = 0; i < kPeriodsPerConfig; i++) {
    EXPECT_EQ(pinger.ExecuteCurrentAction(radio, ping_pipe), AgentAction::kReceive) << " (A) pinger @ " << i;
    EXPECT_EQ(pinger.ExecuteCurrentAction(radio, ping_pipe), AgentAction::kTransmitNextMessage) << "  (B)pinger @ " << i;
  }

  ponger_thread.join();
}

} // namespace
