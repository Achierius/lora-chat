#include "lora_interface.hpp"
#include "sx1276/sx1276.hpp"

namespace lora_chat {

constexpr sx1276::ChannelConfig kHardcodedLoraChannelConfig {
  .freq = 0xe4c000,
  .bw = sx1276::Bandwidth::k125kHz,
  .cr = sx1276::CodingRate::k4_7,
  .sf = sx1276::SpreadingFactor::kSF9,
};

RadioInterface::Status LoraInterface::Transmit(std::span<uint8_t const> buffer) {
  if (fd_ < 0) return Status::kInitializationFailed;
  if (!buffer.size_bytes() || buffer.size_bytes() > SX127x_FIFO_CAPACITY)
    return Status::kBadBufferSize;

  sx1276::lora_transmit(fd_, &buffer[0], buffer.size_bytes());
  return Status::kSuccess;
}

RadioInterface::Status LoraInterface::Receive(std::span<uint8_t> buffer_out) {
  if (fd_ < 0) return Status::kInitializationFailed;
  if (buffer_out.size_bytes() < SX127x_FIFO_CAPACITY)
    return Status::kBadBufferSize;

  bool success = sx1276::lora_receive_continuous(fd_, &buffer_out[0], SX127x_FIFO_CAPACITY);
  // TODO should actually check for whether we got a timeout or something else
  return success ? Status::kSuccess : Status::kTimeout;
}

size_t LoraInterface::MaximumMessageLength() const {
  return SX127x_FIFO_CAPACITY;
}

LoraInterface::LoraInterface() : fd_{spi_init()} {
  sx1276::init_lora(fd_, kHardcodedLoraChannelConfig);
}

} // namespace lora_chat
