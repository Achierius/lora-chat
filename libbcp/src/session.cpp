#include "session.hpp"

#include <thread>

#include <cassert>
#include <cstdio>
#include <cstdlib>

namespace lora_chat {

Session::Clock::Clock(TimePoint start_time, Duration transmission_duration, Duration gap_duration)
  : start_time_(start_time), transmission_duration_(transmission_duration), gap_duration_(gap_duration) {}

Session::Duration Session::Clock::ElapsedTimeInPeriod(Session::TimePoint t) const {
  return (t - start_time_) % TransmissionPeriod();
}

Session::Duration Session::Clock::ElapsedTimeInCurrentPeriod() const {
  return ElapsedTimeInSession() % TransmissionPeriod();
}

Session::Duration Session::Clock::ElapsedTimeInSession() const {
  return std::chrono::steady_clock::now() - start_time_;
}

TransmissionState Session::Clock::InitiatorActionKind(TimePoint t) const {
  const auto elapsed {ElapsedTimeInPeriod(t)};
  if (elapsed < transmission_duration_)
    return TransmissionState::kTransmitting;
  else if (elapsed < transmission_duration_ + gap_duration_)
    return TransmissionState::kInactive;
  else if (elapsed < (transmission_duration_ * 2) + gap_duration_)
    return TransmissionState::kReceiving;
  return TransmissionState::kInactive;
}

TransmissionState Session::Clock::InitiatorActionKind() const {
  return InitiatorActionKind(std::chrono::steady_clock::now());
}

Session::TimePoint Session::Clock::TimeOfNextAction(TimePoint t) const {
  const auto elapsed {ElapsedTimeInPeriod(t)};
  if (elapsed < transmission_duration_)
    return t + transmission_duration_ - elapsed;
  else if (elapsed < transmission_duration_ + gap_duration_)
    return t + transmission_duration_ + gap_duration_ - elapsed;
  else if (elapsed < (transmission_duration_ * 2) + gap_duration_)
    return t + (transmission_duration_ * 2) + gap_duration_ - elapsed;

  assert(TransmissionPeriod() >= elapsed);
  return t + TransmissionPeriod() - elapsed;
}

Session::TimePoint Session::Clock::TimeOfNextAction() const {
  return TimeOfNextAction(std::chrono::steady_clock::now());
}

Session::Session(Session::Id id, Session::Duration transmission_duration, Session::Duration gap_duration)
  : id_(id),
    clock_(std::chrono::steady_clock::now() + kHandshakeLeadTime,
           transmission_duration,
           gap_duration),
    we_initiated_(true) { }

Session::Session(Session::TimePoint start_time, Session::Id id, Session::Duration transmission_duration,
         Session::Duration gap_duration)
  : id_(id),
    clock_(start_time, transmission_duration, gap_duration),
    we_initiated_(false) {
  // TODO check whether the start_time_ is in the past --
  // or insufficiently far in the future?
}

AgentAction Session::WhatToDoRightNow() const {
  return WhatToDoIgnoringCurrentTime(LocalizeActionKind(clock_.InitiatorActionKind()));
}

AgentAction Session::SleepUntilEndOfGapTime() const {
  TimePoint wake_time {};
  if (LocalizeActionKind(clock_.InitiatorActionKind()) == TransmissionState::kInactive)
    wake_time = clock_.TimeOfNextAction();
  else
    wake_time = clock_.TimeOfNextAction(clock_.TimeOfNextAction());

  // Pre-compute what action we'll be doing once we're done sleeping,
  // to save time once we wake up
  AgentAction action = WhatToDoIgnoringCurrentTime(LocalizeActionKind(clock_.InitiatorActionKind(wake_time)));
  std::this_thread::sleep_until(wake_time);
  return action;
}

AgentAction Session::WhatToDoIgnoringCurrentTime(TransmissionState supposed_state) const {
  switch (supposed_state) {
  case TransmissionState::kInactive:
    return AgentAction::kSleepUntilNextAction;
  case TransmissionState::kReceiving:
    return AgentAction::kReceive;
  case TransmissionState::kTransmitting:
    break;
  }

  // Decide what to transmit
  // this is what we would expect the sn to be if we got a message during the
  // last receive sequence
  SequenceNumber expected_recv_sn = we_initiated_ ? last_sent_sn_ : last_sent_sn_ + 1;
  if (last_recv_sn_ == expected_recv_sn) {
    return AgentAction::kTransmitNextMessage;
  } else if (last_recv_sn_ == expected_recv_sn - 1) {
    // We received no messages since our last transmission, NACK so that
    // the counterparty retransmits
    return AgentAction::kTransmitNack;
  } else {
    std::printf("*** error: inconsistent sequence numbers, last-received was %ull, last-sent was %ull\n",
      last_recv_sn_, last_sent_sn_);
    std::exit(-1);
  }
}

TransmissionState Session::LocalizeActionKind(TransmissionState initiator_action_kind) const {
  if (!we_initiated_) {
    switch (initiator_action_kind) {
    case TransmissionState::kInactive:
      return TransmissionState::kInactive;
    case TransmissionState::kReceiving:
      return TransmissionState::kTransmitting;
    case TransmissionState::kTransmitting:
      return TransmissionState::kReceiving;
    }
  }
  return initiator_action_kind;
}

}  // namespace lora_chat
