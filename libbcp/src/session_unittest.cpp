#include "session.hpp"

#include <array>
#include <chrono>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <iostream>

#include "gtest/gtest.h"

namespace {

TEST(ActionTimings, Initiator) {
  using Session = lora_chat::Session;
  using AgentAction = lora_chat::AgentAction;
  using SequenceNumber = lora_chat::SequenceNumber;

  constexpr std::array<std::pair<std::chrono::milliseconds, std::chrono::milliseconds>, 4> kTestConfigs {{
    {std::chrono::milliseconds(10), std::chrono::milliseconds(10)},
    {std::chrono::milliseconds(20), std::chrono::milliseconds(5)},
    {std::chrono::milliseconds(5), std::chrono::milliseconds(20)},
    {std::chrono::milliseconds(15), std::chrono::milliseconds(0)},
  }};
  constexpr int kPeriodsPerConfig {10}; 

  for (auto const& [transmit, gap] : kTestConfigs) {
    Session session {0, transmit, gap};
    SequenceNumber sn = SequenceNumber(0);
    std::this_thread::sleep_for(Session::kHandshakeLeadTime);
    for (int i = 0; i < kPeriodsPerConfig; i++) {
      std::cerr << static_cast<int>(session.WhatToDoRightNow()) << std::endl;
      EXPECT_EQ(session.WhatToDoRightNow(), AgentAction::kTransmitNextMessage);
      session.MarkMessageSend(false);
      std::this_thread::sleep_for(transmit);
      if (gap != std::chrono::milliseconds(0)) {
        std::cerr << static_cast<int>(session.WhatToDoRightNow()) << std::endl;
        EXPECT_EQ(session.WhatToDoRightNow(), AgentAction::kSleepUntilNextAction);
        std::this_thread::sleep_for(gap);
      }
      std::cerr << static_cast<int>(session.WhatToDoRightNow()) << std::endl;
      EXPECT_EQ(session.WhatToDoRightNow(), AgentAction::kReceive);
      session.MarkMessageReceipt(sn);
      sn++;
      std::this_thread::sleep_for(transmit);
      if (gap != std::chrono::milliseconds(0)) {
        std::cerr << static_cast<int>(session.WhatToDoRightNow()) << std::endl;
        EXPECT_EQ(session.WhatToDoRightNow(), AgentAction::kSleepUntilNextAction);
        std::this_thread::sleep_for(gap);
      }
    }
  }
}

TEST(ActionTimings, Follower) {
  using Session = lora_chat::Session;
  using AgentAction = lora_chat::AgentAction;
  using SequenceNumber = lora_chat::SequenceNumber;

  constexpr std::array<std::pair<std::chrono::milliseconds, std::chrono::milliseconds>, 4> kTestConfigs {{
    {std::chrono::milliseconds(10), std::chrono::milliseconds(10)},
    {std::chrono::milliseconds(20), std::chrono::milliseconds(5)},
    {std::chrono::milliseconds(5), std::chrono::milliseconds(20)},
    {std::chrono::milliseconds(15), std::chrono::milliseconds(0)},
  }};
  constexpr int kPeriodsPerConfig {10}; 

  for (auto const& [transmit, gap] : kTestConfigs) {
    Session session {std::chrono::steady_clock::now(), 0, transmit, gap};
    SequenceNumber sn = SequenceNumber(0);
    for (int i = 0; i < kPeriodsPerConfig; i++) {
      EXPECT_EQ(session.WhatToDoRightNow(), AgentAction::kReceive);
      session.MarkMessageReceipt(sn);
      std::this_thread::sleep_for(transmit);
      if (gap != std::chrono::milliseconds(0)) {
        EXPECT_EQ(session.WhatToDoRightNow(), AgentAction::kSleepUntilNextAction);
        std::this_thread::sleep_for(gap);
      }
      EXPECT_EQ(session.WhatToDoRightNow(), AgentAction::kTransmitNextMessage);
      session.MarkMessageSend(false);
      sn++;
      std::this_thread::sleep_for(transmit);
      if (gap != std::chrono::milliseconds(0)) {
        EXPECT_EQ(session.WhatToDoRightNow(), AgentAction::kSleepUntilNextAction);
        std::this_thread::sleep_for(gap);
      }
    }
  }
}

} // namespace
