#pragma once

#include <string>
#include <string_view>

#include "config.hpp"

constexpr size_t kMaxUserInputSize {128};

enum class UserCommandTag {
  kBadCommand,
  kTransmitMessage,
  kTransmitIota,
  kReceiveMessage,
};

using TransmitMessagePayload = std::array<char, kMaxUserInputSize>;  // Message to send
using TransmitIotaPayload = int; // number of messages to send
using ReceiveMessagePayload = int;  // milliseconds to pend

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
