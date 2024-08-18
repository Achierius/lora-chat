#include "protocol_agent.hpp"
#include "packet.hpp"

#include <cassert>
#include <cstring>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

namespace lora_chat {

Duration
ProtocolAgent::AdvertisingClock::ElapsedTimeInPeriod(TimePoint t) const {
  return (t - start_time()) % AdvertisingPeriod();
}

Duration ProtocolAgent::AdvertisingClock::ElapsedTimeInCurrentPeriod() const {
  return ElapsedTimeSinceStart() % AdvertisingPeriod();
}

TransmissionState
ProtocolAgent::AdvertisingClock::ActionKindImpl(TimePoint t) const {
  const auto elapsed{ElapsedTimeInPeriod(t)};
  if (elapsed < advertising_duration_)
    return TransmissionState::kTransmitting;
  else if (elapsed < advertising_duration_ + response_wait_duration_)
    return TransmissionState::kReceiving;
  return TransmissionState::kInactive;
}

TimePoint
ProtocolAgent::AdvertisingClock::TimeOfNextActionImpl(TimePoint t) const {
  const auto elapsed{ElapsedTimeInPeriod(t)};
  const auto t0 = t - elapsed;
  if (elapsed < advertising_duration_)
    return t0 + advertising_duration_;
  else if (elapsed < advertising_duration_ + response_wait_duration_)
    return t0 + advertising_duration_ + response_wait_duration_;
  return t0 + AdvertisingPeriod();
}

void ProtocolAgent::ExecuteAgentAction() {
  // Dispatch is special in that we don't want it to appear to the outside
  // world as an action, as it's really just a way to factor out common logic
  // from the other action-states
  // So one call to ExecuteAgentAction will both dispatch and execute the
  // dispatched action
  if (state_ == ProtocolState::kDispatch)
    DispatchNextState();

  switch (state_) {
  case ProtocolState::kDispatch:
    assert(false && "Dispatch dispatched to the dispatch state!");
  case ProtocolState::kPend:
    Pend();
    return;
  case ProtocolState::kSeek:
    Seek();
    return;
  case ProtocolState::kAdvertise:
    Advertise();
    return;
  case ProtocolState::kExecuteSession:
    ExecuteSession();
    return;
  case ProtocolState::kExecuteHandshakeFromSeek:
    RequestConnection();
    return;
  case ProtocolState::kExecuteHandshakeFromAdvertise:
    AcceptConnection();
    return;
  }
}

void ProtocolAgent::LogPacket(Packet const &p,
                              [[maybe_unused]] WirePacket const &w_p,
                              const char *action) const {
  if constexpr (kLogLevel >= kLogPacketMetadata) {
    auto tid = gettid();
    const char *kIndent = "        ";
    printf("(t%07d: ProtocolAgent) %s packet %s (len %u)\n"
           "%s  sn %03u,  nesn %03u\n",
           tid, action, TypeStr(p.type), p.length, kIndent, p.sn.value,
           p.nesn.value);
    if constexpr (kLogLevel >= kLogPacketBytes) {
      printf("%s[ ", kIndent);
      for (uint8_t i = 0; i < w_p.size(); i++) {
        printf("%02x ", w_p[i]);
      }
      printf("]\n");
    }
  }
}

const char *ProtocolAgent::StateStr(ProtocolState s) const {
  switch (s) {
  case ProtocolState::kDispatch:
    return "<Dispatch>";
  case ProtocolState::kPend:
    return "<Pend>";
  case ProtocolState::kAdvertise:
    return "<Advertise>";
  case ProtocolState::kSeek:
    return "<Seek>";
  case ProtocolState::kExecuteHandshakeFromSeek:
    return "<ExecuteHandshakeFromSeek>";
  case ProtocolState::kExecuteHandshakeFromAdvertise:
    return "<ExecuteHandshakeFromAdvertise>";
  case ProtocolState::kExecuteSession:
    return "<ExecuteSession>";
  }
  assert(false && "bad state");
}

void ProtocolAgent::ChangeState(ProtocolState new_state) {
  if constexpr (kLogLevel >= kLogTransitions) {
    auto tid = gettid();
    printf("(t%07d: ProtocolAgent) State %s -> %s\n", tid, StateStr(state_),
           StateStr(new_state));
  }
  prior_state_ = state_;
  state_ = new_state;
}

void ProtocolAgent::DispatchNextState() {
  auto next_state = [&]() {
    switch (goal_) {
    case ConnectionGoal::kDisconnect:
      return ProtocolState::kPend;
    case ConnectionGoal::kSeekConnection:
      return ProtocolState::kSeek;
    case ConnectionGoal::kAdvertiseConnection:
      return ProtocolState::kAdvertise;
    case ConnectionGoal::kSeekAndAdvertiseConnection:
      if (prior_state_ == ProtocolState::kAdvertise)
        return ProtocolState::kSeek;
      return ProtocolState::kAdvertise;
    }
    assert(false && "Unreachable");
  };
  ChangeState(next_state());
}

void ProtocolAgent::Pend() {
  // TODO use a CV instead of busy-waiting
  std::this_thread::sleep_for(kPendSleepTime);
  ChangeState(ProtocolState::kDispatch);
}

void ProtocolAgent::Seek() {
  WirePacket w_p{};
  auto get_ad = [&]() -> bool {
    auto status = radio_.get().Receive(w_p);
    if (!(status == RadioInterface::Status::kSuccess))
      return false;
    auto p{Packet::Deserialize(w_p)};
    if constexpr (kLogLevel >= kLogPacketMetadata)
      LogPacket(p, w_p, "Received");
    // TODO we should use their ID somewhere
    return p.type == Packet::kAdvertisement;
  };
  bool got_packet = get_ad();
  ChangeState(got_packet ? ProtocolState::kExecuteHandshakeFromSeek
                         : ProtocolState::kDispatch);
}

void ProtocolAgent::RequestConnection() {
  // First we broadcast the request
  Packet conn_req{};
  conn_req.type = Packet::kConnectionRequest;
  conn_req.id = id_;
  auto w_conn_req = conn_req.Serialize();
  auto status = radio_.get().Transmit(w_conn_req);
  assert(status == RadioInterface::Status::kSuccess); // TODO handle err
  if constexpr (kLogLevel >= kLogPacketMetadata)
    LogPacket(conn_req, w_conn_req, "Transmitted");

  // Then we wait for the result
  WirePacket w_p{};
  auto receive_begin = Now();
  do {
    auto status = radio_.get().Receive(w_p);
    if (status != RadioInterface::Status::kSuccess)
      continue;
    Packet response{Packet::Deserialize(w_p)};
    if constexpr (kLogLevel > kNone)
      LogPacket(response, w_p, "Received");
    if (response.type == Packet::kConnectionAccept) {
      // TODO confirm they're the party we expect
      // The payload contains our session's start time
      assert(response.length == sizeof(WireTimePoint));
      WireTimePoint wire_time{};
      std::memcpy(&wire_time, &response.payload, sizeof(wire_time));
      TimePoint start_time(DeserializeWireTime(wire_time));
      session_.emplace(start_time, id_, kHardcodedTransmissionTime,
                       kHardcodedSleepTime, false);
      // Success!
      ChangeState(ProtocolState::kExecuteSession);
      session_->SleepUntilStartTime();
      return;
    }
  } while (Now() - receive_begin < kHandshakeReceiveDuration);

  // Disappointment: no response
  ChangeState(ProtocolState::kDispatch);
}

void ProtocolAgent::Advertise() {
  // First we broadcast the advertisement
  Packet conn_req{};
  conn_req.type = Packet::kAdvertisement;
  conn_req.id = id_;
  auto w_conn_req = conn_req.Serialize();
  auto status = radio_.get().Transmit(w_conn_req);
  assert(status == RadioInterface::Status::kSuccess); // TODO handle err
  if constexpr (kLogLevel >= kLogPacketMetadata)
    LogPacket(conn_req, w_conn_req, "Transmitted");

  // Then we wait for the result
  WirePacket w_p{};
  auto receive_begin = Now();
  do {
    auto status = radio_.get().Receive(w_p);
    if (status != RadioInterface::Status::kSuccess)
      continue;
    Packet response{Packet::Deserialize(w_p)};
    if constexpr (kLogLevel > kNone)
      LogPacket(response, w_p, "Received");
    if (response.type == Packet::kConnectionRequest) {
      // Success: got a fish!
      ChangeState(ProtocolState::kExecuteHandshakeFromAdvertise);
      return;
    }
  } while (Now() - receive_begin < kConnectionRequestInterval);

  // Disappointment: empty line
  ChangeState(ProtocolState::kDispatch);
}

void ProtocolAgent::AcceptConnection() {
  Packet p{};
  p.type = Packet::kConnectionAccept;
  p.id = id_;
  p.length = sizeof(WireTimePoint);

  WireTimePoint wire_start_time =
      GetFutureWireTime(kHandshakeLeadTime);
  std::memcpy(&p.payload, &wire_start_time, sizeof(wire_start_time));

  auto start_time = DeserializeWireTime(wire_start_time);
  session_.emplace(start_time, id_, kHardcodedTransmissionTime,
                   kHardcodedSleepTime, true);

  auto w_p = p.Serialize();
  if constexpr (kLogLevel >= kLogPacketMetadata)
    LogPacket(p, w_p, "Transmitted");
  auto status = radio_.get().Transmit(w_p);
  if (status != RadioInterface::Status::kSuccess) {
    // TODO retry sending connection-accept instead of just giving up
    ChangeState(ProtocolState::kPend);
    return;
  }

  ChangeState(ProtocolState::kExecuteSession);
  session_->SleepUntilStartTime();
}

void ProtocolAgent::ExecuteSession() {
  assert(session_.has_value() && "Bad protocol state");
  if (session_->ExecuteCurrentAction(radio_.get(), pipe_) ==
      AgentAction::kSessionComplete) {
    ChangeState(ProtocolState::kPend);
  }
  if (goal_ == ConnectionGoal::kDisconnect) {
    // TODO disconnect gracefully instead of letting them time out
    ChangeState(ProtocolState::kPend);
  }
}

} // namespace lora_chat
