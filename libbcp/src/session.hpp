#pragma once

#include <chrono>
#include <limits>

#include <cstdint>

namespace lora_chat {

enum class TransmissionState {
  kInactive,
  kReceiving,
  kTransmitting,
};

enum class AgentAction {
  kSleepUntilNextAction,
  kReceive,
  kTransmitNextMessage,
  kTransmitNack,
  // TODO support ending the session
};

using WireSessionTime = uint64_t;
using SequenceNumber = uint8_t; // bluetooth uses 1 bit, but how does that
                                // handle time-delayed reflections?

class Session {
public:
  using Id = uint16_t;
  using TimePoint = std::chrono::steady_clock::time_point;
  using Duration = std::chrono::steady_clock::duration;
  static constexpr Duration kHandshakeLeadTime { std::chrono::microseconds(1'000'000) };

private:
  // Each session is tied to an implicit clock which is synchronized between the
  // two agents within said session. At a given time-point within the session,
  // both the initiator and the follower must agree as to what action each will
  // be taking -- transmitting, receiving, or nothing at all.
  // Assuming ideal actors (i.e. each can begin to transmit/receive instantly,
  // do so for a precise amount of time, and thereafter sleep for a precise
  // amount of time, without any clock drift between the two actors).
  // time,
  // The initiator will first transmit for `transmit_duration`, then sleep for
  // `gap_duration`, then receive for `transmit_duration`, then sleep for
  // `gap_duration`, then repeat.
  // The follower will first receive for `transmit_duration`, then sleep for
  // `gap_duration`, then transmit for `transmit_duration`, then sleep for
  // `gap_duration`, then repeat.
  // N.b. currently I don't resynchronize after startup, so drift is possible.
  class Clock {
    // TODO support resychronization to account for clock drift
    // TODO account for initial clock skew between the two actors -- can
    // possibly measure during the handshake by measuring the ping time each way
    // and then seeing what the delta is when that is accounted for.
    // The "transmission period" Tp is the interval between when the session's
    // initiator should begin transmitting the Nth message and when it should
    // begin transmitting the N+1th message.
    Duration TransmissionPeriod() const { return 2 * (transmission_duration_ + gap_duration_); }

    /// Returns the time elapsed since the start of the transmission period
    /// to which the time point belongs
    Duration ElapsedTimeInPeriod(TimePoint t) const;
    Duration ElapsedTimeInCurrentPeriod() const;

  public:
    Clock(TimePoint start_time, Duration transmission_duration, Duration gap_duration);

    /// Returns the time elapsed since the start of this session.
    /// This is NOT equal to the time elapsed since this object was created.
    Duration ElapsedTimeInSession() const;

    /// Returns what kind of action the initiator of this session should be
    /// undertaking at `time`.
    /// We can't know the exact action since that will depend on details like
    /// the values of the sequence numbers at `t`.
    TransmissionState InitiatorActionKind(TimePoint t) const;
    TransmissionState InitiatorActionKind() const;

    /// Returns the first time at which an agent, starting at time `t`,
    /// should switch to another action.
    TimePoint TimeOfNextAction(TimePoint t) const;
    TimePoint TimeOfNextAction() const;

  private:
    TimePoint start_time_;
    Duration transmission_duration_;
    Duration gap_duration_;
  };

public:
  /// For sessions we initiate -- we transmit first, and starting at every time
  /// t ≡ 0 (mod Tp)
  Session(Id id, Duration transmission_duration, Duration gap_duration);
  /// For sessions initiated by the counterparty -- we receive first, and
  /// transmit second as well as at every time
  /// t ≡ Tp/2 (mod Tp)
  Session(TimePoint start_time, Id id, Duration transmission_duration,
          Duration gap_duration);

  /// Returns the specific action an agent executing this session should
  /// start doing right now.
  AgentAction WhatToDoRightNow() const;

  /// Sleeps the current thread until the next time at which
  /// WhatToDoRightNow would not return either the current action or kInactive.
  /// N.b. this means that if the current action is e.g. kReceiving, this
  /// function will sleep through both the remainder of the reception block as
  /// well as the following gap-time.
  /// Returns the action to take upon waking.
  AgentAction SleepUntilEndOfGapTime() const;

private:
  constexpr static SequenceNumber kInitialSn {std::numeric_limits<SequenceNumber>::max()};

  /// Decides what we'd do if we were transmitting/receiving/etc. (according to
  /// `supposed_state`) given the current values of our stored sequence numbers.
  /// For kReceive/kInactive this is 1-to-1, but for kTransmit we might do
  /// different things -- send a message, send a nack, end the session,
  /// send a sync packet, etc.
  AgentAction WhatToDoIgnoringCurrentTime(TransmissionState supposed_state) const;

  /// Returns what action kind the local agent should do at the time the
  /// initiating agent is doing `initiator_action_kind`.
  TransmissionState LocalizeActionKind(TransmissionState initiator_action_kind) const;

  Id id_;

  Clock clock_;

  SequenceNumber last_recv_sn_ {kInitialSn - 1};
  SequenceNumber last_sent_sn_ {kInitialSn};
  SequenceNumber last_acked_sent_sn {kInitialSn - 3};

  uint64_t messages_sent_ {0};

  bool we_initiated_;
};

}  // namespace lora_chat
