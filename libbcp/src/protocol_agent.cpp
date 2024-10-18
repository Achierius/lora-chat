#include "protocol_agent.hpp"
#include "packet.hpp"

#include <cassert>
#include <cstdarg>
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

void ProtocolAgent::LogStr(const char* format, ...) const {
  char buffer[512];

  va_list args;
  va_start(args, format);

  vsnprintf(buffer, sizeof(buffer), format, args);

  va_end(args);

  printf("(t%07d: ProtocolAgent) %s\n", gettid(), buffer);
}


void ProtocolAgent::LogPacket(Packet<PacketType::kSession> const &p,
                              [[maybe_unused]] std::span<const uint8_t> w_p,
                              const char *action) const {
  if constexpr (kLogLevel >= kLogPacketMetadata) {
    const char *kIndent = "        ";
    LogStr("%s Session packet %s (len %u)\n"
           "%s  sn %03u,  nesn %03u\n",
           action, TypeStr(p.type), p.length, kIndent, p.sn.value,
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

void ProtocolAgent::LogPacket(Packet<PacketType::kAdvertising> const &p,
                              [[maybe_unused]] std::span<const uint8_t> w_p,
                              const char *action) const {
  if constexpr (kLogLevel >= kLogPacketMetadata) {
    const char *kIndent = "        ";
    LogStr("%s Advertising packet from 0x%08x\n",
           action, p.source_address);

    if constexpr (kLogLevel >= kLogPacketBytes) {
      printf("%s[ ", kIndent);
      for (uint8_t i = 0; i < w_p.size(); i++) {
        printf("%02x ", w_p[i]);
      }
      printf("]\n");
    }
  }
}

void ProtocolAgent::LogPacket(Packet<PacketType::kConnectionRequest> const &p,
                              [[maybe_unused]] std::span<const uint8_t> w_p,
                              const char *action) const {
  if constexpr (kLogLevel >= kLogPacketMetadata) {
    const char *kIndent = "        ";
    LogStr("%s Connection-Request packet from 0x%08x\n",
           action, p.source_address);

    if constexpr (kLogLevel >= kLogPacketBytes) {
      printf("%s[ ", kIndent);
      for (uint8_t i = 0; i < w_p.size(); i++) {
        printf("%02x ", w_p[i]);
      }
      printf("]\n");
    }
  }
}

void ProtocolAgent::LogPacket(Packet<PacketType::kConnectionAccept> const &p,
                              [[maybe_unused]] std::span<const uint8_t> w_p,
                              const char *action) const {
  if constexpr (kLogLevel >= kLogPacketMetadata) {
    const char *kIndent = "        ";
    LogStr("%s Connection-Accept packet from 0x%08x\n"
           "%s  session-id will be %u\n",
           action, p.source_address, kIndent, p.session_id);
    // TODO log start-time & frequency

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

std::pair<RadioInterface::Status, ReceiveBuffer> ProtocolAgent::ReceivePacket() {
  ReceiveBuffer buff{};
  auto status = radio_.get().Receive(buff.span());

  // TODO rearchitect internals to avoid this unnecessary copy
  ReceiveBuffer w_p{};
  std::memcpy(&w_p, buff.data(), buff.size());
  return {status, w_p};
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
  auto get_ad = [&]() -> bool {
    auto [status, w_p] = ReceivePacket();
    if (!(status == RadioInterface::Status::kSuccess)) {
      LogStr("failed to receive packet in seek: %d", status);
      return false;
    }
    // TODO re-seek if we get a non-ad packet?
    auto maybe_ad = Deserialize<PacketType::kAdvertising>(w_p);
    if (!maybe_ad) return false;
    auto ad = maybe_ad.value();

    if constexpr (kLogLevel >= kLogPacketMetadata)
      LogPacket(ad, w_p.span(), "Received");
    // TODO we should use their ID somewhere
    return true;
  };
  bool got_packet = get_ad();
  ChangeState(got_packet ? ProtocolState::kExecuteHandshakeFromSeek
                         : ProtocolState::kDispatch);
}

void ProtocolAgent::RequestConnection() {
  Packet<PacketType::kConnectionRequest> conn_req{};
  conn_req.source_address = address_;
  conn_req.target_address = 0; // TODO plumb in target-addr

  auto w_conn_req = Serialize(conn_req);
  auto status = radio_.get().Transmit(w_conn_req);
  assert(status == RadioInterface::Status::kSuccess); // TODO handle err
  if constexpr (kLogLevel >= kLogPacketMetadata)
    LogPacket(conn_req, w_conn_req, "Transmitted");

  // Then we wait for the result
  auto receive_begin = Now();
  do {
    auto [status, w_p] = ReceivePacket();
    if (status != RadioInterface::Status::kSuccess) {
      LogStr("failed to receive connection-accept: %d", status);
      continue;
    }
    auto maybe_response = Deserialize<PacketType::kConnectionAccept>(w_p);
    if (!maybe_response) continue;
    auto response = maybe_response.value();

    if constexpr (kLogLevel > kNone)
      LogPacket(response, w_p.span(), "Received");
    // TODO confirm they're the party we expect

    TimePoint start_time(DeserializeWireTime(response.session_start_time));
    session_.emplace(start_time, address_, kHardcodedTransmissionTime,
                     kHardcodedSleepTime, false);
    // Success!
    ChangeState(ProtocolState::kExecuteSession);
    session_->SleepUntilStartTime();
    return;
  } while (Now() - receive_begin < kHandshakeReceiveDuration);

  // Disappointment: no response
  if constexpr (kLogLevel > kNone)
    LogStr("connection-request failed", status);
  ChangeState(ProtocolState::kDispatch);
}

void ProtocolAgent::Advertise() {
  // First we broadcast the advertisement
  Packet<PacketType::kAdvertising> advert{};
  advert.source_address = address_;
  auto w_advert = Serialize(advert);
  auto status = radio_.get().Transmit(w_advert);
  assert(status == RadioInterface::Status::kSuccess); // TODO handle err
  if constexpr (kLogLevel >= kLogPacketMetadata)
    LogPacket(advert, w_advert, "Transmitted");

  // Then we wait for the result
  auto receive_begin = Now();
  do {
    auto [status, w_p] = ReceivePacket();
    if (status != RadioInterface::Status::kSuccess)
      continue;
    auto maybe_response = Deserialize<PacketType::kConnectionRequest>(w_p);
    if (!maybe_response) continue;

    // Success: got a fish!
    auto response = maybe_response.value();
    if constexpr (kLogLevel > kNone)
      LogPacket(response, w_p.span(), "Received");
    ChangeState(ProtocolState::kExecuteHandshakeFromAdvertise);
    return;
  } while (Now() - receive_begin < kConnectionRequestInterval);

  // Disappointment: empty line
  ChangeState(ProtocolState::kDispatch);
}

void ProtocolAgent::AcceptConnection() {
  Packet<PacketType::kConnectionAccept> accept{};
  accept.source_address = address_;
  accept.target_address = 0;  // TODO pipe in target address
  accept.session_start_time = GetFutureWireTime(kHandshakeLeadTime);
  accept.session_id = address_;  // TODO generate session IDs

  auto start_time = DeserializeWireTime(accept.session_start_time);
  session_.emplace(start_time, address_, kHardcodedTransmissionTime,
                   kHardcodedSleepTime, true);

  auto w_accept = Serialize(accept);
  if constexpr (kLogLevel >= kLogPacketMetadata)
    LogPacket(accept, w_accept, "Transmitted");
  auto status = radio_.get().Transmit(w_accept);
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
