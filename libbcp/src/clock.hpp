#pragma once

#include <cassert>
#include <chrono>

#include "time.hpp"

namespace lora_chat {

enum class TransmissionState {
  kInactive,
  kReceiving,
  kTransmitting,
};

class Clock {
public:
  Clock(TimePoint start_time) : start_time_(start_time) {}
  virtual ~Clock() = default;

  /// Returns the time elapsed since the start of this session.
  /// This is NOT necessarily equal to the time since this object was created.
  Duration ElapsedTimeSinceStart() const { return Now() - start_time_; }

  /// Returns what kind of action the user of this clock should be
  /// undertaking at `time`.
  TransmissionState ActionKind() const { return ActionKind(Now()); }

  TransmissionState ActionKind(TimePoint t) const {
    assert(t >= start_time_ &&
           "Cannot take action before a clock's start-time");
    return ActionKindImpl(t);
  }

  TimePoint TimeOfNextAction() const { return TimeOfNextAction(Now()); }

  TimePoint TimeOfNextAction(TimePoint t) const {
    assert(t >= start_time_ &&
           "Cannot take action before a clock's start-time");
    return TimeOfNextActionImpl(t);
  }

  TimePoint start_time() const { return start_time_; }

private:
  virtual TimePoint TimeOfNextActionImpl(TimePoint t) const = 0;
  virtual TransmissionState ActionKindImpl(TimePoint t) const = 0;

  TimePoint start_time_;
};

} // namespace lora_chat
