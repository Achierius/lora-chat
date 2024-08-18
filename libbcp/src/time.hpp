#pragma once

#include <algorithm>
#include <bit>
#include <chrono>
#include <thread>

// Courtesy of https://stackoverflow.com/questions/809902/64-bit-ntohl-in-c
#if defined(__linux__)
#  include <endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__)
#  include <sys/endian.h>
#elif defined(__OpenBSD__)
#  include <sys/types.h>
#  define be16toh(x) betoh16(x)
#  define be32toh(x) betoh32(x)
#  define be64toh(x) betoh64(x)
#endif

namespace lora_chat {

// For clocking within the protocol
using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::steady_clock::duration;
inline TimePoint Now() { return std::chrono::steady_clock::now(); }

// For communicating over the wire ONLY
// In this case, we don't care as much about the monotonicity: just that both
// devices have a common conception of when the start-time should be.
// TODO just do the RTT math in the handshake rather than relying on NTP
using WireTimePoint = uint64_t;
using WireTimeClock = std::chrono::system_clock;
using WireTimeUnit = std::chrono::nanoseconds;
static_assert(sizeof(time_t) == sizeof(WireTimePoint));

template <typename T>
constexpr T FlipBitsIfBigEndian (T value) noexcept
{
  // Use little-endian on network to avoid unneeded work in our most common case
  if constexpr (std::endian::native == std::endian::big) {
    char* ptr = reinterpret_cast<char*>(&value);
    std::reverse(ptr, ptr + sizeof(T));
  }
  return value;
}

inline WireTimePoint GetFutureWireTime(Duration delay) {
  auto future_time = WireTimeClock::now() + delay;
  WireTimePoint future_time_count = std::chrono::duration_cast<WireTimeUnit>(future_time.time_since_epoch()).count();
  return FlipBitsIfBigEndian(future_time_count);
}

inline TimePoint DeserializeWireTime(WireTimePoint t) {
  auto wire_time_count = WireTimeUnit(FlipBitsIfBigEndian(t));
  auto wire_time = WireTimeClock::time_point(wire_time_count);
  auto wire_clock_now = WireTimeClock::now();
  auto local_now = Now();
  return (wire_time - wire_clock_now) + local_now;
}

} // namespace lora_chat
