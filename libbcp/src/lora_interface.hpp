#pragma once

#include <span>

#include <cstdint>

#include "radio_interface.hpp"

namespace lora_chat {

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
