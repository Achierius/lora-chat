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

class FallibleLocalRadio : public lora_chat::RadioInterface {
public:
  FallibleLocalRadio(std::chrono::milliseconds timeout, int transmission_failure_period, int reception_failure_period)
    : radio_{timeout},
      transmission_failure_period_(transmission_failure_period),
      reception_failure_period_(reception_failure_period) {
    assert(transmission_failure_period_ >= 0);
    assert(reception_failure_period_ >= 0);
  }

  Status Transmit(std::span<uint8_t const> buffer) {
    if (transmission_failure_period_) {
      transmission_failure_counter_ = (transmission_failure_counter_ + 1) % transmission_failure_period_;
      if (!transmission_failure_counter_) return Status::kTimeout;
    }
    return radio_.Transmit(buffer);
  }

  Status Receive(std::span<uint8_t> buffer_out) {
    if (reception_failure_period_) {
      reception_failure_counter_ = (reception_failure_counter_ + 1) % reception_failure_period_;
      if (!reception_failure_counter_) return Status::kTimeout;
    }
    return radio_.Receive(buffer_out);
  }

  size_t MaximumMessageLength() const { return radio_.MaximumMessageLength(); }

private:
  LocalRadio radio_;
  int transmission_failure_period_;
  int transmission_failure_counter_ {0};
  int reception_failure_period_;
  int reception_failure_counter_ {0};
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

struct TextTag { const char *str; };
using MakeMessageType = std::optional<lora_chat::WirePacketPayload>(*)();

template <TextTag const&Tag>
std::optional<lora_chat::WirePacketPayload> MakeMessage() {
  lora_chat::WirePacketPayload p {0};

  static std::atomic<int> i {0};
  std::stringstream ss{};
  ss << Tag.str << " " << i++;
  std::strcpy(reinterpret_cast<char*>(&p), ss.str().c_str());

  return std::optional<lora_chat::WirePacketPayload>(p);
}

template <TextTag const&Tag>
void ConsumeMessage([[maybe_unused]] lora_chat::WirePacketPayload &&msg) {
  constexpr bool kVerbose = false;
  if constexpr (kVerbose)
    printf("%s received message: \"%s\"\n", Tag.str, reinterpret_cast<const char*>(&msg));
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
