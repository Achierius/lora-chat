#pragma once

#include "sx1276/sx1276.hpp"

struct Config {
  sx1276::Frequency frequency;
  sx1276::Bandwidth bandwidth;
  sx1276::CodingRate coding_rate;
  sx1276::SpreadingFactor spreading_factor;
};

// TODO use libfmt and specify print formats for the struct members

// TODO actually prompt the user rather than returning a hardcoded config
// TODO does this really belong here, or should it be in lora_interface?
Config prompt_user_for_config();
