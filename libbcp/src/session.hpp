#pragma once

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "clock.hpp"
#include "packet.hpp"
#include "radio_interface.hpp"
#include "sequence_number.hpp"
#include "time.hpp"

namespace lora_chat {

enum class AgentAction {
  kSleepUntilNextAction,
  kReceive,
  kTransmitNextMessage,
  kRetransmitMessage,
  kTransmitNack,
  kSessionComplete,
  // TODO support ending the session
};

class MessagePipe {
  // Any other asynchronous status update callbacks go here
public:
  using GetMessageFunc = std::optional<WirePacketPayload> (*)();
  using ReceiveMessageFunc = void (*)(WirePacketPayload &&);

  MessagePipe() : get_msg_(DontSendAMessage), recv_msg_(DropMessage) {}

  MessagePipe(GetMessageFunc get_msg)
      : get_msg_(get_msg), recv_msg_(DropMessage) {}

  MessagePipe(GetMessageFunc get_msg, ReceiveMessageFunc recv_msg)
      : get_msg_(get_msg), recv_msg_(recv_msg) {}

  std::optional<WirePacketPayload> GetNextMessageToSend();
  void DepositReceivedMessage(WirePacketPayload &&message);

private:
  GetMessageFunc get_msg_;
  ReceiveMessageFunc recv_msg_;

  static std::optional<WirePacketPayload> DontSendAMessage() { return {}; }
  static void DropMessage(WirePacketPayload &&) { return; }
};

class Session {
public:
  using Id = WireSessionId;

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
  class SessionClock : public Clock {
  public:
    // TODO support resychronization to account for clock drift
    // TODO account for initial clock skew between the two actors -- can
    // possibly measure during the handshake by measuring the ping time each way
    // and then seeing what the delta is when that is accounted for.
    SessionClock(TimePoint start_time, Duration transmission_duration,
                 Duration gap_duration)
        : Clock(start_time), transmission_duration_(transmission_duration),
          gap_duration_(gap_duration) {}

  private:
    // The "transmission period" Tp is the interval between when the session's
    // initiator should begin transmitting the Nth message and when it should
    // begin transmitting the N+1th message.
    Duration TransmissionPeriod() const {
      return 2 * (transmission_duration_ + gap_duration_);
    }

    /// Returns the time elapsed since the start of the transmission period
    /// to which the time point belongs
    Duration ElapsedTimeInPeriod(TimePoint t) const;
    Duration ElapsedTimeInCurrentPeriod() const;

    /// Returns what kind of action the *initiator* of this session should be
    /// undertaking at `time`. The follower has to convert this into their own
    /// local action-kind before it can use it.
    /// We can't know the exact action since that will depend on details like
    /// the values of the sequence numbers at `t`.
    TransmissionState ActionKindImpl(TimePoint t) const override;

    /// Returns the first time at which an agent, starting at time `t`,
    /// should switch to another action.
    TimePoint TimeOfNextActionImpl(TimePoint t) const override;

    Duration transmission_duration_;
    Duration gap_duration_;
  };

public:
  /// For sessions we initiate -- we transmit first, and starting at every
  /// time t ≡ 0 (mod Tp)
  /// For sessions initiated by the counterparty -- we receive first, and
  /// transmit second as well as at every time
  /// t ≡ Tp/2 (mod Tp)
  Session(TimePoint start_time, Id id, Duration transmission_duration,
          Duration gap_duration, bool we_initiated);

  /// Executes the action which the session expects for the current time.
  AgentAction ExecuteCurrentAction(RadioInterface &radio, MessagePipe &pipe);

  /// Sleep the current thread until this session is ready to begin executing.
  /// May return immediately if the session is already ready.
  void SleepUntilStartTime() const;

private:
  enum LogLevel {
    kNone = 0,
    kLogPacketMetadata,
    kLogPacketAscii,
    kLogPacketBytes,
  };
  static constexpr LogLevel kLogLevel{kNone};

  /// Returns the specific action an agent executing this session should
  /// start doing right now.
  AgentAction WhatToDoRightNow() const;

  /// Decides what we'd do if we were transmitting/receiving/etc. (according to
  /// `supposed_state`) given the current values of our stored sequence numbers.
  /// For kReceive/kInactive this is 1-to-1, but for kTransmit we might do
  /// different things -- send a message, send a nack, end the session,
  /// send a sync packet, etc.
  AgentAction
  WhatToDoIgnoringCurrentTime(TransmissionState supposed_state) const;

  /// Returns what action kind the local agent should do at the time the
  /// initiating agent is doing `initiator_action_kind`.
  TransmissionState
  LocalizeActionKind(TransmissionState initiator_action_kind) const;

  // TODO I really want to encapsulate these somehow...
  void TransmitNack(RadioInterface &radio, MessagePipe &pipe);
  void TransmitNextMessage(RadioInterface &radio, MessagePipe &pipe);
  void ReceiveMessage(RadioInterface &radio, MessagePipe &pipe);
  void RetransmitMessage(RadioInterface &radio, MessagePipe &pipe);

  /// Sleeps the current thread until the next time at which
  /// WhatToDoRightNow would not return either the current action or kInactive.
  /// N.b. this means that if the current action is e.g. kReceiving, this
  /// function will sleep through both the remainder of the reception block as
  /// well as the following gap-time.
  /// Returns the action to take upon waking.
  AgentAction SleepThroughNextGapTime() const;

  /// Waits until time t to return.
  /// If the remaining time is short enough, does not actually sleep the current
  /// thread: just spins until we hit it instead.
  void SleepUntil(TimePoint t) const;

  void LogForPacket(Packet const &p, WirePacket const &w_p,
                    const char *action) const;

  static SequenceNumber InitFictitiousLastAckedSentSn(bool we_initiated);
  static SequenceNumber InitFictitiousPrevSentNesn(bool we_initiated);

  Id id_;

  SessionClock clock_;

  // When a packet is received, we cannot be sure that it is final until a
  // packet with greater sn is received -- this is because the transmitter could
  // miss our reply and decide to retransmit, potentially retransmitting a
  // packet with different contents, even if only due to EMI.
  SequenceNumber last_recv_sn_{SequenceNumber::kMaximumValue};
  SequenceNumber last_acked_sent_sn_;
  bool received_good_packet_in_last_receive_sequence_{true};
  // We buffer this so that we can retransmit it
  Packet last_sent_packet_;
  // We buffer this and only hand it back out when it's about to be overridden
  WirePacketPayload last_recv_message_{};

  uint64_t messages_sent_{0};

  bool we_initiated_;
};

} // namespace lora_chat
