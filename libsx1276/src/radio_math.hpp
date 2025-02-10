#pragma once

#include "types.hpp"

namespace sx1276 {

const uint16_t kPreambleLengthBytes = 8;
const uint8_t kSyncWordValue = 0x12;
const bool kEnablePayloadCrc = false;

uint32_t bandwidth_in_hz(Bandwidth bw);

uint32_t compute_time_on_air_ms(int msg_bytes, ChannelConfig const& config);

} // namespace sx1276
