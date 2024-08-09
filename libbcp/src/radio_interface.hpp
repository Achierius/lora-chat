#pragma once

#include <span>

#include <cstdint>

namespace lora_chat {

class RadioInterface {
public:
  virtual ~RadioInterface() = default;

  enum Status {
    kSuccess,
    kTimeout,
    kBadBufferSize,
    kBadMessage,
    kInitializationFailed,
    kUnspecifiedError,
  };

  virtual Status Transmit(std::span<uint8_t const> buffer) = 0;
  virtual Status Receive(std::span<uint8_t> buffer_out) = 0;

  virtual size_t MaximumMessageLength() const = 0;
};

} // namespace lora_chat
