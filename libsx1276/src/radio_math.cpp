#include "radio_math.hpp"

#include <cassert>
#include <cmath>

namespace sx1276 {

uint32_t bandwidth_in_hz(Bandwidth bw) {
  switch (bw) {
    case Bandwidth::k7_8kHz:
      return 7800;
    case Bandwidth::k10_4kHz:
      return 10400;
    case Bandwidth::k15_6kHz:
      return 15600;
    case Bandwidth::k20_8kHz:
      return 20800;
    case Bandwidth::k31_25kHz:
      return 31250;
    case Bandwidth::k41_7kHz:
      return 41700;
    case Bandwidth::k62_5kHz:
      return 62500;
    case Bandwidth::k125kHz:
      return 125000;
    case Bandwidth::k250kHz:
      return 250000;
    case Bandwidth::k500kHz:
      return 500000;
  }
  return bw;
}

namespace {
float symbol_duration_s(ChannelConfig const& config) {
  auto bw_hz = bandwidth_in_hz(config.bw) * 1.0f;
  return (1 << config.sf) / bw_hz;
}

bool low_data_rate_optimization_is_mandated(ChannelConfig const& config) {
  return symbol_duration_s(config) > 16e-3;
}

float payload_length_symbols(uint32_t n_msg, ChannelConfig const& config) {
  // This computation is all from page 31 of Semtech's datasheet for the SX1276/77/78/79
  // https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R0000001Rbr/6EfVZUorrpoKFfvaF_Fkpgp5kzjiNyiAbqcpqh9qSjE
  
  auto& [freq, bw, cr, sf] = config;
  auto adjusted_sf = sf - (low_data_rate_optimization_is_mandated(config) ? 2 : 0);
  auto cr_expansion_factor = cr + 4;

  // 5 is for the explicit header: if we ever start using implicit it goes away
  auto n_overhead = 2 + (kEnablePayloadCrc ? 4 : 0) + 5;
  // In the manual this is given with an extra multiple of 4 applied to the
  // numerator and denominator, but AFAICT it's not necessary -- and doesn't
  // actually get optimized out b/c floating point rules
  auto base_length = std::max(1.0f, ceilf((2 * n_msg - sf + n_overhead) / adjusted_sf));
  auto full_length = 8 + (base_length * cr_expansion_factor);

  return full_length;
}

float preamble_length_symbols() {
  return kPreambleLengthBytes + 4.25f;
}

} // namespace

uint32_t compute_time_on_air_ms(int msg_bytes, ChannelConfig const& config) {
  // Using the raw ToA calculations gave me flaky results -- it might be because
  // we don't/can't wait for the RxDone IRQ?
  // 50 was mostly fine but I noticed a few failures so we're playing it safe.
  constexpr uint32_t kTimeOnAirFudgeFactorMs = 75;

  assert(msg_bytes > 0);
  auto total_symbols = preamble_length_symbols() +
    payload_length_symbols(static_cast<uint32_t>(msg_bytes), config);
  float time_on_air_s = symbol_duration_s(config) * total_symbols;

  return (time_on_air_s * 1000) + kTimeOnAirFudgeFactorMs;
}

} // namespace sx1276
