#include "session.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include "clock.hpp"
#include "sequence_number.hpp"

namespace lora_chat {

std::optional<WirePacketPayload> MessagePipe::GetNextMessageToSend() {
  return get_msg_();
}
void MessagePipe::DepositReceivedMessage(WirePacketPayload &&message) {
  return recv_msg_(std::move(message));
}

Duration Session::SessionClock::ElapsedTimeInPeriod(TimePoint t) const {
  return (t - start_time()) % TransmissionPeriod();
}

Duration Session::SessionClock::ElapsedTimeInCurrentPeriod() const {
  return ElapsedTimeSinceStart() % TransmissionPeriod();
}

TransmissionState Session::SessionClock::ActionKindImpl(TimePoint t) const {
  const auto elapsed{ElapsedTimeInPeriod(t)};
  if (elapsed < transmission_duration_)
    return TransmissionState::kTransmitting;
  else if (elapsed < transmission_duration_ + gap_duration_)
    return TransmissionState::kInactive;
  else if (elapsed < (transmission_duration_ * 2) + gap_duration_)
    return TransmissionState::kReceiving;
  return TransmissionState::kInactive;
}

TimePoint Session::SessionClock::TimeOfNextActionImpl(TimePoint t) const {
  const auto elapsed{ElapsedTimeInPeriod(t)};
  const auto t0 = t - elapsed;
  if (elapsed < transmission_duration_)
    return t0 + transmission_duration_;
  else if (elapsed < transmission_duration_ + gap_duration_)
    return t0 + transmission_duration_ + gap_duration_;
  else if (elapsed < (transmission_duration_ * 2) + gap_duration_)
    return t0 + (transmission_duration_ * 2) + gap_duration_;

  assert(TransmissionPeriod() >= elapsed);
  return t0 + TransmissionPeriod();
}

Session::Session(TimePoint start_time, Session::Id id,
                 Duration transmission_duration, Duration gap_duration,
                 bool we_initiated)
    : id_(id), clock_(start_time, transmission_duration, gap_duration),
      last_acked_sent_sn_(InitFictitiousLastAckedSentSn(we_initiated)),
      last_sent_packet_{.id = id_,
                        .length = 0,
                        .nesn = InitFictitiousPrevSentNesn(we_initiated),
                        .sn = SequenceNumber(SequenceNumber::kMaximumValue)},
      we_initiated_(we_initiated) {
  // TODO check whether the start_time_ is in the past --
  // or insufficiently far in the future?
}

SequenceNumber Session::InitFictitiousLastAckedSentSn(bool we_initiated) {
  return SequenceNumber(we_initiated ? SequenceNumber::kMaximumValue
                                     : SequenceNumber::kMaximumValue - 1);
}

SequenceNumber Session::InitFictitiousPrevSentNesn(bool we_initiated) {
  return SequenceNumber(we_initiated ? SequenceNumber::kMaximumValue : 0);
}

AgentAction Session::WhatToDoRightNow() const {
  return WhatToDoIgnoringCurrentTime(LocalizeActionKind(clock_.ActionKind()));
}

AgentAction Session::ExecuteCurrentAction(RadioInterface &radio,
                                          MessagePipe &pipe) {
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
  case AgentAction::kTerminateSession:
    TerminateSession(radio, pipe);
    break;
  case AgentAction::kSleepUntilNextAction:
  case AgentAction::kSessionComplete:
    break;
  }
  return SleepThroughNextGapTime();
}

AgentAction Session::SleepThroughNextGapTime() const {
  TimePoint wake_time = clock_.TimeOfNextAction();
  if (LocalizeActionKind(clock_.ActionKind(wake_time)) ==
      TransmissionState::kInactive)
    wake_time = clock_.TimeOfNextAction(clock_.TimeOfNextAction());

  // Pre-compute what action we'll be doing once we're done sleeping,
  // to save time once we wake up
  AgentAction action = WhatToDoIgnoringCurrentTime(
      LocalizeActionKind(clock_.ActionKind(wake_time)));
  // The action should not be 'sleep more': if that's the case, we should just
  // sleep for a longer duration
  assert(action != AgentAction::kSleepUntilNextAction);
  SleepUntil(wake_time);
  return action;
}

void Session::SleepUntilStartTime() const { SleepUntil(clock_.start_time()); }

void Session::TransmitNack(RadioInterface &radio, MessagePipe &pipe) {
  Packet p{};
  p.type = Packet::kNack;
  p.nesn = last_recv_sn_ + 1;
  p.sn = last_sent_packet_.sn;
  p.id = id_;
  p.length = 0;

  auto w_p = p.Serialize();
  if constexpr (kLogLevel > kNone)
    LogForPacket(p, w_p, "Transmitted NACK");
  radio.Transmit(w_p);
  timeout_counter_++;
}

void Session::TransmitNextMessage(RadioInterface &radio, MessagePipe &pipe) {
  Packet &p = last_sent_packet_;
  p.type = Packet::kData;
  p.nesn = last_recv_sn_ + 1;
  p.sn = last_acked_sent_sn_ + 1;
  p.id = id_;

  auto message = pipe.GetNextMessageToSend();
  if (message) {
    p.length = message.value().size();
    std::memcpy(&p.payload, message.value().data(), message.value().size());
  } else {
    p.length = 0;
  }

  auto w_p = p.Serialize();
  if constexpr (kLogLevel > kNone)
    LogForPacket(p, w_p, "Transmitted");
  radio.Transmit(w_p);
}

void Session::ReceiveMessage(RadioInterface &radio, MessagePipe &pipe) {
  received_good_packet_in_last_receive_sequence_ = false;
  ReceiveBuffer buff{};
  WirePacket &w_p = buff.packet;
  // TODO repeat receive until we get the proper session id
  // TODO enforce timeout according to how long we're supposed to receive for
  auto status = radio.Receive(buff.Span());
  if (status != RadioInterface::Status::kSuccess) {
    // TODO do we need to do anything special for bad packets?
    return;
  }
  received_good_packet_in_last_receive_sequence_ = true;
  timeout_counter_ = 0;
  Packet p{Packet::Deserialize(w_p)};
  if constexpr (kLogLevel > kNone)
    LogForPacket(p, w_p, "Received");

  if (p.nesn == static_cast<SequenceNumber>(last_sent_packet_.sn + 1)) {
    last_acked_sent_sn_ = last_sent_packet_.sn;

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
    last_recv_sn_ = p.sn;
  } else if (p.type == Packet::kNack && p.nesn == last_sent_packet_.sn) {
    // If p.nesn matches but p.type != kNack, it's a reflected/desync'd data msg
    // If p.type is kNack but p.nesn is something else, same but a nack

    // Do nothing, they want us to retransmit
  } else {
    // Something bad happened!
    assert(false && "Bad protocol state");
  }
}

void Session::RetransmitMessage(RadioInterface &radio, MessagePipe &pipe) {
  // TODO how to handle it when they nack our nack?
  auto w_p = last_sent_packet_.Serialize();
  if constexpr (kLogLevel > kNone)
    LogForPacket(last_sent_packet_, w_p, "Retransmitted");
  radio.Transmit(w_p);
}

void Session::TerminateSession(RadioInterface &, MessagePipe &) {
  session_complete_ = true;
  // TODO flush buffers
  // TODO send a termination packet
}

void Session::SleepUntil(TimePoint t) const {
  // May need to be higher depending on the target system
  constexpr Duration kSpinloopThreshold = std::chrono::milliseconds(5);

  // TODO strictly we should be switching behavior based on the duration of the
  // event taking place AFTER we wake up
  if (t - std::chrono::steady_clock::now() >= kSpinloopThreshold) {
    std::this_thread::sleep_until(t);
  } else {
    while (t > std::chrono::steady_clock::now()) {
      std::atomic_signal_fence(std::memory_order_seq_cst);
    }
  }
}

void Session::LogForPacket(Packet const &p,
                           [[maybe_unused]] WirePacket const &w_p,
                           const char *action) const {
  if constexpr (kLogLevel >= kLogPacketMetadata) {
    auto tid = gettid();
    const char *kIndent = "        ";
    const char *role = we_initiated_ ? "Initiator" : "Follower";
    printf("(t%07d: Session %s) %s packet %s (len %u)\n"
           "%s  sn %03u,  nesn %03u\n"
           "%slrsn %03u,  lssn %03u\n"
           "%s          lassn %03u\n",
           tid, role, action, TypeStr(p.type), p.length, kIndent, p.sn.value,
           p.nesn.value, kIndent, last_recv_sn_.value,
           last_sent_packet_.sn.value, kIndent, last_acked_sent_sn_.value);
    if constexpr (kLogLevel >= kLogPacketBytes) {
      printf("%s[ ", kIndent);
      for (uint8_t i = 0; i < w_p.size(); i++) {
        printf("%02x ", w_p[i]);
      }
      printf("]\n");
    }
    if constexpr (kLogLevel >= kLogPacketAscii) {
      if (p.type == Packet::kData)
        printf("%s\"%s\"\n", kIndent, p.payload.data());
    }
  }
}

AgentAction
Session::WhatToDoIgnoringCurrentTime(TransmissionState supposed_state) const {
  if (session_complete_) return AgentAction::kSessionComplete;

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
    return(timeout_counter_ <= kTimeoutLimit) ? AgentAction::kTransmitNack : AgentAction::kTerminateSession;

  // Decide what to transmit
  // this is what we would expect the sn to be if we got a message during the
  // last receive sequence
  if (last_acked_sent_sn_ == last_sent_packet_.sn)
    return AgentAction::kTransmitNextMessage;
  else if (last_acked_sent_sn_ + 1 == last_sent_packet_.sn)
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

} // namespace lora_chat
