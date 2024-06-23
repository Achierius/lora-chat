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

class LoraInterface : public RadioInterface {
public:
  LoraInterface(const LoraInterface &) = delete;
  LoraInterface &operator=(const LoraInterface &) = delete;
  LoraInterface(LoraInterface &&) = delete;
  LoraInterface &operator=(LoraInterface &&) = delete;

  static LoraInterface &instance() {
    static LoraInterface instance;
    return instance;
  }

  virtual Status Transmit(std::span<uint8_t const> buffer);
  virtual Status Receive(std::span<uint8_t> buffer_out);

  virtual size_t MaximumMessageLength() const;

private:
  LoraInterface();

  int fd_;
};

} // namespace lora_chat
