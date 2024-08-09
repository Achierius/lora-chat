#include "session.hpp"

#include <array>
#include <chrono>
#include <limits>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <cstdio>

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

TEST(ActionTimings, Initiator) {
  using Session = lora_chat::Session;
  using AgentAction = lora_chat::AgentAction;

  constexpr std::array<std::pair<std::chrono::milliseconds, std::chrono::milliseconds>, 4> kTestConfigs {{
    {std::chrono::milliseconds(10), std::chrono::milliseconds(10)},
    {std::chrono::milliseconds(20), std::chrono::milliseconds(5)},
    {std::chrono::milliseconds(5), std::chrono::milliseconds(20)},
    {std::chrono::milliseconds(15), std::chrono::milliseconds(7)},
  }};
  constexpr int kPeriodsPerConfig {10}; 

  for (auto const& [transmit, gap] : kTestConfigs) {
    CountingRadio radio{};
    lora_chat::MessagePipe pipe{};
    Session session {0, transmit, gap};
    std::this_thread::sleep_for(Session::kHandshakeLeadTime);
    for (int i = 0; i < kPeriodsPerConfig; i++) {
      // this is the NEXT action the session will take
      EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive);
      EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{1, 0}));
      EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kTransmitNextMessage);
      EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 1}));
    }
  }
}

TEST(ActionTimings, Follower) {
  using Session = lora_chat::Session;
  using AgentAction = lora_chat::AgentAction;

  constexpr std::array<std::pair<std::chrono::milliseconds, std::chrono::milliseconds>, 4> kTestConfigs {{
    {std::chrono::milliseconds(10), std::chrono::milliseconds(10)},
    {std::chrono::milliseconds(20), std::chrono::milliseconds(5)},
    {std::chrono::milliseconds(5), std::chrono::milliseconds(20)},
    {std::chrono::milliseconds(15), std::chrono::milliseconds(7)},
  }};
  constexpr int kPeriodsPerConfig {10}; 

  for (auto const& [transmit, gap] : kTestConfigs) {
    CountingRadio radio{};
    lora_chat::MessagePipe pipe{};
    Session session {std::chrono::steady_clock::now(), 0, transmit, gap};
    for (int i = 0; i < kPeriodsPerConfig; i++) {
      // this is the NEXT action the session will take
      EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kTransmitNextMessage) << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{0, 1})) << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(session.ExecuteCurrentAction(radio, pipe), AgentAction::kReceive) << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
      EXPECT_EQ(radio.GetAndClearObservedActions(), (std::pair{1, 0})) << " -- transmit: " << transmit << " gap: " << gap << " i: " << i;
    }
  }
}

} // namespace
