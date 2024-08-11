#include "session.hpp"

#include <thread>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace lora_chat {

std::optional<WirePacketPayload> MessagePipe::GetNextMessageToSend() {
  return get_msg_();
}
void MessagePipe::DepositReceivedMessage(WirePacketPayload &&message) {
  return recv_msg_(std::move(message));
}

Session::Clock::Clock(TimePoint start_time, Duration transmission_duration,
                      Duration gap_duration)
    : start_time_(start_time), transmission_duration_(transmission_duration),
      gap_duration_(gap_duration) {}

Session::Duration
Session::Clock::ElapsedTimeInPeriod(Session::TimePoint t) const {
  assert(t >= start_time_);
  return (t - start_time_) % TransmissionPeriod();
}

Session::Duration Session::Clock::ElapsedTimeInCurrentPeriod() const {
  return ElapsedTimeInSession() % TransmissionPeriod();
}

Session::Duration Session::Clock::ElapsedTimeInSession() const {
  return std::chrono::steady_clock::now() - start_time_;
}

TransmissionState Session::Clock::InitiatorActionKind(TimePoint t) const {
  assert(t >= start_time_ && "A session cannot do an action before it starts");
  const auto elapsed{ElapsedTimeInPeriod(t)};
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
  if (t < start_time_)
    return start_time_;

  const auto elapsed{ElapsedTimeInPeriod(t)};
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

Session::Session(Session::Id id, Session::Duration transmission_duration,
                 Session::Duration gap_duration)
    : id_(id), clock_(std::chrono::steady_clock::now() + kHandshakeLeadTime,
                      transmission_duration, gap_duration),
      we_initiated_(true) {}

Session::Session(Session::TimePoint start_time, Session::Id id,
                 Session::Duration transmission_duration,
                 Session::Duration gap_duration)
    : id_(id), clock_(start_time, transmission_duration, gap_duration),
      we_initiated_(false) {
  // TODO check whether the start_time_ is in the past --
  // or insufficiently far in the future?
}

AgentAction Session::WhatToDoRightNow() const {
  return WhatToDoIgnoringCurrentTime(
      LocalizeActionKind(clock_.InitiatorActionKind()));
}

AgentAction Session::ExecuteCurrentAction(RadioInterface &radio, MessagePipe &pipe) {
  auto action = WhatToDoRightNow();
  switch (action) {
  case AgentAction::kReceive:
    ReceiveMessage(radio, pipe);
    break;
  case AgentAction::kTransmitNextMessage:
    TransmitNextMessage(radio, pipe);
    break;
  case AgentAction::kTransmitNack:
    TransmitNack(radio, pipe);
    break;
  case AgentAction::kRetransmitMessage:
    RetransmitMessage(radio, pipe);
    break;
  case AgentAction::kSleepUntilNextAction:
    break;
  }
  return SleepThroughNextGapTime();
}

AgentAction Session::SleepThroughNextGapTime() const {
  TimePoint wake_time = clock_.TimeOfNextAction();
  if (LocalizeActionKind(clock_.InitiatorActionKind(wake_time)) ==
      TransmissionState::kInactive)
    wake_time = clock_.TimeOfNextAction(clock_.TimeOfNextAction());

  // Pre-compute what action we'll be doing once we're done sleeping,
  // to save time once we wake up
  AgentAction action = WhatToDoIgnoringCurrentTime(
      LocalizeActionKind(clock_.InitiatorActionKind(wake_time)));
  // The action should not be 'sleep more': if that's the case, we should just
  // sleep for a longer duration
  assert(action != AgentAction::kSleepUntilNextAction);
  std::this_thread::sleep_until(wake_time);
  return action;
}

void Session::TransmitNack(RadioInterface &radio, MessagePipe &pipe) {
  Packet p{};
  p.nesn = last_recv_sn_ + 1;
  p.sn = last_sent_sn_ + 1;  // TODO should nacks actually advance the SN?
  p.id = id_;
  p.length = 0;

  radio.Transmit(p.Serialize());
}

void Session::TransmitNextMessage(RadioInterface &radio, MessagePipe &pipe) {
  Packet p{};
  p.nesn = last_recv_sn_ + 1;
  p.sn = last_sent_sn_ + 1;
  p.id = id_;

  // TODO track the length of messages
  auto message = pipe.GetNextMessageToSend();
  if (message) {
    p.length = message.value().size();
    std::memcpy(&p.payload, message.value().data(), message.value().size());
  } else {
    p.length = 0;
  }

  radio.Transmit(p.Serialize());
}

void Session::ReceiveMessage(RadioInterface &radio, MessagePipe &pipe) {
  received_good_packet_in_last_receive_sequence_ = false;
  WirePacket w_p {};
  // TODO repeat receive until we get the proper session id
  auto status = radio.Receive(w_p);
  if (!(status == RadioInterface::Status::kSuccess)) {
    // TODO do we need to do anything special for bad packets?
    return;
  }
  received_good_packet_in_last_receive_sequence_ = true;

  Packet p {Packet::Deserialize(w_p)};
  if (p.nesn == static_cast<SequenceNumber>(last_sent_sn_ + 1)) {
    last_acked_sent_sn_ = last_sent_sn_;

    if (p.sn == last_recv_sn_) {
      // For whatever reason, they're retransmitting their last
      // message -- even though we already received it.
      // TODO Is this retransmit case legal??
      last_recv_message_ = std::move(p.payload);
      // If so, we don't propogate out the old message since it was logically
      // overridden by the new one with the same SN
    } else if (p.sn == last_recv_sn_ + 1) {
      pipe.DepositReceivedMessage(std::move(last_recv_message_));
      last_recv_message_ = std::move(p.payload);
    }
  } else if (p.nesn == last_sent_sn_) {
    // Do nothing, they want us to retransmit
  } else {
    // Something bad happened!
    assert(false && "Bad protocol state");
  }
}

void Session::RetransmitMessage(RadioInterface &radio, MessagePipe &pipe) {
  // TODO how to handle it when they nack our nack?

}

AgentAction
Session::WhatToDoIgnoringCurrentTime(TransmissionState supposed_state) const {
  switch (supposed_state) {
  case TransmissionState::kInactive:
    return AgentAction::kSleepUntilNextAction;
  case TransmissionState::kReceiving:
    return AgentAction::kReceive;
  case TransmissionState::kTransmitting:
    break;
  }

  // If we received no messages since our last transmission, NACK so that
  // the counterparty retransmits
  if (!received_good_packet_in_last_receive_sequence_)
    return AgentAction::kTransmitNack;

  // Decide what to transmit
  // this is what we would expect the sn to be if we got a message during the
  // last receive sequence
  if (last_acked_sent_sn_ == last_sent_sn_)
    return AgentAction::kTransmitNextMessage;
  else if (last_acked_sent_sn_ + 1 == last_sent_sn_)
    return AgentAction::kRetransmitMessage;
  else
    assert(false && "Unreachable");
}

TransmissionState
Session::LocalizeActionKind(TransmissionState initiator_action_kind) const {
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

} // namespace liora_chat
