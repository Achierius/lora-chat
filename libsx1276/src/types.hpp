#pragma once

#include <cstdint>

namespace sx1276 {

enum OpMode : uint8_t {
  kSleep = 0x0,
  kStandby = 0x1,
  kFrequencySynthesisTransmit = 0x2,
  kTransmit = 0x3,
  kFrequencySynthesisReceive = 0x4,
  kReceiveContinuous = 0x5,
  kReceiveSingle = 0x6,
  kChannelActivityDetection = 0x7,
};

enum Bandwidth : uint8_t {
  k7_8kHz = 0,
  k10_4kHz = 1,
  k15_6kHz = 2,
  k20_8kHz = 3,
  k31_25kHz = 4,
  k41_7kHz = 5,
  k62_5kHz = 6,
  k125kHz = 7,
  k250kHz = 8,
  k500kHz = 9,
};

enum CodingRate : uint8_t {
  kUndefinedCodingRate = 0,
  k4_5 = 1,
  k4_6 = 2,
  k4_7 = 3,
  k4_8 = 4,
};

enum SpreadingFactor : uint8_t  {
  kUndefinedSpreadingFactor = 0,
  kSF6 = 6,
  kSF7 = 7,
  kSF8 = 8,
  kSF9 = 9,
  kSF10 = 10,
  kSF11 = 11,
  kSF12 = 12,
};

// TODO make proper types for this
using Frequency = uint32_t;

struct ChannelConfig {
  Frequency freq;
  Bandwidth bw;
  CodingRate cr;
  SpreadingFactor sf;
};

} // namespace sx1276
