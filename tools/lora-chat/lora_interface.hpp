#pragma once

#include <optional>
#include <string>
#include <utility>

#include "config.hpp"

bool init_lora(Config const& cfg);

enum class TransmitStatus {
  kSuccess,
  kUnspecifiedError,
  kBadInput,
};
TransmitStatus lora_transmit(const char* msg, size_t msg_len);

enum class ReceiveStatus {
  kSuccess,
  kUnspecifiedError,
  kBadInput,
  kNoMessage,
};
std::pair<ReceiveStatus, std::optional<std::string>> lora_receive();
