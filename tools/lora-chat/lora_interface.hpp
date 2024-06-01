#pragma once

#include "config.hpp"

enum class LoraStatus {
  kOk,
  kUnspecifiedError,
  kBadInput,
};

LoraStatus init_lora(Config const& cfg);

LoraStatus lora_transmit(const char* msg, size_t msg_len);
