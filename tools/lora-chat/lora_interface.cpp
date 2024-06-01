#include "lora_interface.hpp"

#include <optional>

#include "sx1276/sx1276.hpp"

struct LoraState {
  int fd;
  Config cfg;
};

namespace {
std::optional<LoraState> lora_state {};
}

LoraStatus init_lora(Config const& cfg) {
  // do not reinitialize if the fd is already open
  if (lora_state) return LoraStatus::kUnspecifiedError;

  int fd = spi_init();
  lora_state = {fd, cfg};
  sx1276::init_lora(fd, cfg.frequency, cfg.bandwidth, cfg.coding_rate, cfg.spreading_factor);

  return LoraStatus::kOk;
}

LoraStatus lora_transmit(const char* msg, size_t msg_len) {
  if (!msg || !msg_len || msg_len > SX127x_FIFO_CAPACITY)
    return LoraStatus::kBadInput;
  if (!lora_state)
    return LoraStatus::kUnspecifiedError;

  // With default SF/CR/BW/etc. the minimum is around 97 for 1-3 chars;
  // 4-8 needs 125 to be solid. Conservatively set the base @ 150 + the
  // slope at 7/byte.
  // TODO adapt for different configuration values
  // TODO TOA calculation should live inside the lora library
  int time_on_air = 150 + (7 * msg_len);
  
  sx1276::lora_transmit(lora_state->fd, time_on_air, reinterpret_cast<const uint8_t*>(msg), msg_len);
  return LoraStatus::kOk;
}
