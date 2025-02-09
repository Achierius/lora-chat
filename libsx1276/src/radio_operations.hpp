#pragma once

#include "spi_wrappers.hpp"
#include "sx1276_lora_registers.hpp"
#include "types.hpp"

namespace sx1276 {

void init_lora(int fd, Frequency freq, Bandwidth bw, CodingRate cr, SpreadingFactor sf);

// TODO propogate errors
void lora_transmit(int fd, int time_on_air_ms, const uint8_t* msg, int len);
bool lora_receive_single(int fd, int time_on_air_ms, uint8_t* dest, int max_len);
bool lora_receive_continuous(int fd, int time_on_air_ms, uint8_t* dest, int max_len);

} // namespace sx1276
