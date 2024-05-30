#pragma once

#include "spi_wrappers.hpp"
#include "sx1276_lora_registers.hpp"

namespace Sx127x {

// TODO make an enum for spreading factor
void init_lora(int fd, uint32_t freq, Sx127x::Bandwidth bw, Sx127x::CodingRate
    cr, int spreading_factor);

// TODO propogate errors
void lora_transmit(int fd, int time_on_air_ms, const uint8_t* msg, int len);

} // namespace Sx127x
