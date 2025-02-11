#include "config.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr sx1276::Frequency kDefaultFrequency{0xe4c000};
constexpr sx1276::Bandwidth kDefaultBandwidth{sx1276::Bandwidth::k125kHz};
constexpr sx1276::CodingRate kDefaultCodingRate{sx1276::CodingRate::k4_7};
constexpr sx1276::SpreadingFactor kDefaultSpreadingFactor{sx1276::SpreadingFactor::kSF9};

}; // namespace

Config prompt_user_for_config() {
  Config conf{
    .channel = sx1276::ChannelConfig {
      .freq = kDefaultFrequency,
      .bw = kDefaultBandwidth,
      .cr = kDefaultCodingRate,
      .sf = kDefaultSpreadingFactor,
    },
  };

  // TODO use libfmt and make formatters for the struct types
  printf("Using hardcoded configuration:\n"
         "\tfrequency         0x%x\n"
         "\tbandwidth         %d\n"
         "\tcoding-rate       %d\n"
         "\tspreading-factor  %d\n",
         conf.channel.freq, conf.channel.bw,
         conf.channel.cr, conf.channel.sf);

  return conf;
}
