#pragma once

#include <string>
#include <string_view>

#include "config.hpp"
#include "sx1276/sx1276.hpp"

constexpr size_t kMaxUserInputSize {SX127x_FIFO_CAPACITY * 2};

enum class UserCommandTag {
  kBadCommand,
  kTransmitMessage,
  kTransmitIota,
  kReceiveMessage,
};

using TransmitMessagePayload = std::array<char, kMaxUserInputSize>;  // Message to send
using TransmitIotaPayload = int; // number of messages to send
// TODO use radio-facing gpio to detect reception and ditch the manual ToA spec
using ReceiveMessagePayload = std::pair<int, int>;  // number of messages to
                                                    // consume, milliseconds to pend

// I could use std::variant but I really don't feel like
// dealing with that today
struct UserCommand {
  union {
    TransmitMessagePayload as_transmit_message;
    ReceiveMessagePayload as_receive_message;
  };
  UserCommandTag tag;
};

UserCommand get_and_parse_user_input();
