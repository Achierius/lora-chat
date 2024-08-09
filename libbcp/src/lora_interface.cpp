#include "lora_interface.hpp"
#include "sx1276/sx1276.hpp"

namespace lora_chat {

constexpr sx1276::Frequency kLoraFrequency{0xe4c000};
constexpr sx1276::Bandwidth kLoraBandwidth{sx1276::Bandwidth::k125kHz};
constexpr sx1276::CodingRate kLoraCodingRate{sx1276::CodingRate::k4_7};
constexpr sx1276::SpreadingFactor kLoraSpreadingFactor{9};

RadioInterface::Status LoraInterface::Transmit(std::span<uint8_t const> buffer) {
  if (fd_ < 0) return Status::kInitializationFailed;
  if (!buffer.size_bytes() || buffer.size_bytes() > SX127x_FIFO_CAPACITY)
    return Status::kBadBufferSize;

  // yoinked from tools/lora-chat/lora_interface.cpp
  // TODO TOA calculation should live inside the lora library
  int time_on_air = 150 + (7 * buffer.size_bytes());

  sx1276::lora_transmit(fd_, time_on_air, &buffer[0], buffer.size_bytes());
  return Status::kSuccess;
}

RadioInterface::Status LoraInterface::Receive(std::span<uint8_t> buffer_out) {
  if (fd_ < 0) return Status::kInitializationFailed;
  if (buffer_out.size_bytes() < SX127x_FIFO_CAPACITY)
    return Status::kBadBufferSize;

  // TODO TOA calculation should live inside the lora library
  constexpr int kTimeOnAir = 150 + (7 * SX127x_FIFO_CAPACITY);

  bool success = sx1276::lora_receive_single(fd_, kTimeOnAir, &buffer_out[0], SX127x_FIFO_CAPACITY);
  // TODO should actually check for whether we got a timeout or something else
  return success ? Status::kSuccess : Status::kTimeout;
}

size_t LoraInterface::MaximumMessageLength() const {
  return SX127x_FIFO_CAPACITY;
}

LoraInterface::LoraInterface() : fd_{spi_init()} {
  sx1276::init_lora(fd_, kLoraFrequency, kLoraBandwidth, kLoraCodingRate,
                    kLoraSpreadingFactor);
}

} // namespace lora_chat
