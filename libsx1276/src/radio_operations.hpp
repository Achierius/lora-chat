#pragma once

#include "spi_wrappers.hpp"
#include "sx1276_lora_registers.hpp"
#include "types.hpp"

namespace sx1276 {

void init_lora(int fd, ChannelConfig config);
bool get_channel_config(int fd, ChannelConfig* config);

// TODO propogate errors
void lora_transmit(int fd, const uint8_t* msg, int len);
bool lora_receive_single(int fd, uint8_t* dest, int max_len);
bool lora_receive_continuous(int fd, uint8_t* dest, int max_len);

} // namespace sx1276
