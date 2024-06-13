#include "lora_interface.hpp"

#include <optional>

#include "sx1276/sx1276.hpp"

struct LoraState {
  int fd;
  Config cfg;
};

namespace {
std::optional<LoraState> lora_state{};
}

bool init_lora(Config const &cfg) {
  // do not reinitialize if the fd is already open
  if (lora_state)
    return false;

  int fd = spi_init();
  lora_state = {fd, cfg};
  sx1276::init_lora(fd, cfg.frequency, cfg.bandwidth, cfg.coding_rate,
                    cfg.spreading_factor);

  return true;
}

TransmitStatus lora_transmit(const char *msg, size_t msg_len) {
  if (!msg || !msg_len || msg_len > SX127x_FIFO_CAPACITY)
    return TransmitStatus::kBadInput;
  if (!lora_state)
    return TransmitStatus::kUnspecifiedError;

  // With default SF/CR/BW/etc. the minimum is around 97 for 1-3 chars;
  // 4-8 needs 125 to be solid. Conservatively set the base @ 150 + the
  // slope at 7/byte.
  // TODO adapt for different configuration values
  // TODO TOA calculation should live inside the lora library
  int time_on_air = 150 + (7 * msg_len);

  sx1276::lora_transmit(lora_state->fd, time_on_air,
                        reinterpret_cast<const uint8_t *>(msg), msg_len);
  return TransmitStatus::kSuccess;
}

std::pair<ReceiveStatus, std::optional<std::string>> lora_receive(int pend_time_ms) {
  if (pend_time_ms < 100)
    return {ReceiveStatus::kBadInput, {}};
  if (!lora_state)
    return {ReceiveStatus::kUnspecifiedError, {}};

  std::array<uint8_t, SX127x_FIFO_CAPACITY> buff{0};
  auto got_msg = sx1276::lora_receive(lora_state->fd, pend_time_ms, buff.data(),
                                      SX127x_FIFO_CAPACITY);
  if (got_msg) {
    std::string str(buff.begin(), buff.end());
    return {ReceiveStatus::kSuccess, str};
  } else {
    return {ReceiveStatus::kNoMessage, {}};
  }
}
