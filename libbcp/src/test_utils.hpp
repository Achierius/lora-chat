#pragma once

#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <optional>
#include <semaphore>
#include <thread>
#include <tuple>
#include <utility>

#include "packet.hpp"
#include "radio_interface.hpp"

namespace lora_chat::testutils {

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
  std::pair<int, int> observed_actions_{0, 0};
};

class LocalRadio : public lora_chat::RadioInterface {
public:
  // TODO this is a bit of a cludge, prefer a proper barrier w/ timeout
  LocalRadio(std::chrono::milliseconds timeout) : timeout_(timeout) {}

  Status Transmit(std::span<uint8_t const> buffer) {
    if (buffer.size() > MaximumMessageLength())
      return Status::kBadBufferSize;
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
    if (!transmission_ready_.try_acquire_for(timeout_))
      return Status::kTimeout;
    if (buffer_out.size() < inflight_buffer_.size())
      return Status::kBadBufferSize;
    {
      std::scoped_lock lock(buffer_lock_);
      std::copy(inflight_buffer_.begin(), inflight_buffer_.end(),
                buffer_out.begin());
    }
    return Status::kSuccess;
  }

  size_t MaximumMessageLength() const { return 1 << 10; }

private:
  std::mutex transmission_lock_{};
  std::binary_semaphore transmission_ready_{0};
  std::mutex buffer_lock_{};
  std::span<uint8_t const> inflight_buffer_{};
  std::chrono::milliseconds timeout_;
};

class FallibleLocalRadio : public lora_chat::RadioInterface {
public:
  FallibleLocalRadio(std::chrono::milliseconds timeout,
                     int transmission_failure_period,
                     int reception_failure_period)
      : radio_{timeout},
        transmission_failure_period_(transmission_failure_period),
        reception_failure_period_(reception_failure_period) {
    assert(transmission_failure_period_ >= 0);
    assert(reception_failure_period_ >= 0);
  }

  Status Transmit(std::span<uint8_t const> buffer) {
    if (transmission_failure_period_) {
      transmission_failure_counter_ =
          (transmission_failure_counter_ + 1) % transmission_failure_period_;
      if (!transmission_failure_counter_)
        return Status::kTimeout;
    }
    return radio_.Transmit(buffer);
  }

  Status Receive(std::span<uint8_t> buffer_out) {
    if (reception_failure_period_) {
      reception_failure_counter_ =
          (reception_failure_counter_ + 1) % reception_failure_period_;
      if (!reception_failure_counter_)
        return Status::kTimeout;
    }
    return radio_.Receive(buffer_out);
  }

  size_t MaximumMessageLength() const { return radio_.MaximumMessageLength(); }

private:
  LocalRadio radio_;
  int transmission_failure_period_;
  int transmission_failure_counter_{0};
  int reception_failure_period_;
  int reception_failure_counter_{0};
};

struct TextTag {
  const char *str;
};
using MakeMessageType = std::optional<lora_chat::WirePacketPayload> (*)();

template <TextTag const &Tag>
std::optional<lora_chat::WirePacketPayload> MakeMessage() {
  lora_chat::WirePacketPayload p{0};

  static std::atomic<int> i{0};
  std::stringstream ss{};
  ss << Tag.str << " " << i++;
  std::strcpy(reinterpret_cast<char *>(&p), ss.str().c_str());

  return std::optional<lora_chat::WirePacketPayload>(p);
}

template <TextTag const &Tag>
void ConsumeMessage([[maybe_unused]] lora_chat::WirePacketPayload &&msg) {
  constexpr bool kVerbose = false;
  if constexpr (kVerbose)
    printf("%s received message: \"%s\"\n", Tag.str,
           reinterpret_cast<const char *>(&msg));
}

} // namespace lora_chat::testutils
