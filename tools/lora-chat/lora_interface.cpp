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
  sx1276::init_lora(fd, cfg.channel);

  return true;
}

TransmitStatus lora_transmit(const char *msg, size_t msg_len) {
  if (!msg || !msg_len || msg_len > SX127x_FIFO_CAPACITY)
    return TransmitStatus::kBadInput;
  if (!lora_state)
    return TransmitStatus::kUnspecifiedError;

  sx1276::lora_transmit(lora_state->fd, reinterpret_cast<const uint8_t *>(msg), msg_len);
  return TransmitStatus::kSuccess;
}

std::pair<ReceiveStatus, std::optional<std::string>> lora_receive() {
  if (!lora_state)
    return {ReceiveStatus::kUnspecifiedError, {}};

  std::array<uint8_t, SX127x_FIFO_CAPACITY> buff{0};
  auto got_msg = sx1276::lora_receive_continuous(lora_state->fd, buff.data(),
                                      SX127x_FIFO_CAPACITY);
  if (got_msg) {
    std::string str(buff.begin(), buff.end());
    return {ReceiveStatus::kSuccess, str};
  } else {
    return {ReceiveStatus::kNoMessage, {}};
  }
}
