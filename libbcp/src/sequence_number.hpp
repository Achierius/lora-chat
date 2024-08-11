#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace lora_chat {

struct SequenceNumber {
  // bluetooth uses 1 bit, but how does that handle time-delayed reflections?
  uint8_t value;

  SequenceNumber() = default;
  explicit SequenceNumber(uint8_t v) : value(v) {}

  const static decltype(value) kMaximumValue = 
    std::numeric_limits<decltype(value)>::max();

  friend std::strong_ordering operator<=>(const SequenceNumber &lhs,
                                          const SequenceNumber &rhs) = default;

  friend SequenceNumber operator+(const SequenceNumber &lhs, uint8_t rhs) {
    return SequenceNumber(static_cast<uint8_t>(lhs.value + rhs));
  }

  friend SequenceNumber operator-(const SequenceNumber &lhs, uint8_t rhs) {
    return SequenceNumber(static_cast<uint8_t>(lhs.value - rhs));
  }

  friend SequenceNumber operator+(const SequenceNumber &lhs,
                                  const SequenceNumber &rhs) {
    return SequenceNumber(static_cast<uint8_t>(lhs.value + rhs.value));
  }

  friend SequenceNumber operator-(const SequenceNumber &lhs,
                                  const SequenceNumber &rhs) {
    return SequenceNumber(static_cast<uint8_t>(lhs.value - rhs.value));
  }

  friend SequenceNumber &operator++(SequenceNumber &sn) {
    ++sn.value;
    return sn;
  }

  friend SequenceNumber operator++(SequenceNumber &sn, int) {
    SequenceNumber temp = sn;
    ++sn;
    return temp;
  }

  friend SequenceNumber &operator--(SequenceNumber &sn) {
    --sn.value;
    return sn;
  }

  friend SequenceNumber operator--(SequenceNumber &sn, int) {
    SequenceNumber temp = sn;
    --sn;
    return temp;
  }

  friend SequenceNumber &operator+=(SequenceNumber &lhs, uint8_t rhs) {
    lhs.value = static_cast<uint8_t>(lhs.value + rhs);
    return lhs;
  }

  friend SequenceNumber &operator-=(SequenceNumber &lhs, uint8_t rhs) {
    lhs.value = static_cast<uint8_t>(lhs.value - rhs);
    return lhs;
  }

  friend SequenceNumber &operator+=(SequenceNumber &lhs,
                                    const SequenceNumber &rhs) {
    lhs.value = static_cast<uint8_t>(lhs.value + rhs.value);
    return lhs;
  }

  friend SequenceNumber &operator-=(SequenceNumber &lhs,
                                    const SequenceNumber &rhs) {
    lhs.value = static_cast<uint8_t>(lhs.value - rhs.value);
    return lhs;
  }
};

} // namespace lora_chat
