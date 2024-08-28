#pragma once

#include <atomic>
#include <cassert>
#include <chrono>

#include "clock.hpp"
#include "packet.hpp"
#include "radio_interface.hpp"
#include "session.hpp"
#include "time.hpp"

namespace lora_chat {

// TODO move MessagePipe over here from session.hpp

class ProtocolAgent {
private:
  using Id = WireSessionId;
  enum class ProtocolState {
    kDispatch,
    kPend,
    kAdvertise,
    kSeek,
    kExecuteHandshakeFromSeek,
    kExecuteHandshakeFromAdvertise,
    kExecuteSession,
  };

  // TODO I should actually use this xd
  class AdvertisingClock : public Clock {
  public:
    AdvertisingClock(TimePoint start_time, Duration advertising_duration,
                     Duration response_wait_duration, Duration sleep_duration)
        : Clock(start_time), advertising_duration_(advertising_duration),
          response_wait_duration_(response_wait_duration),
          sleep_duration_(sleep_duration) {}

    Duration AdvertisingPeriod() const {
      return advertising_duration_ + response_wait_duration_ + sleep_duration_;
    }

  private:
    // Not factoring this out with Clock because I'm going to have to add
    // randomness to this implementation eventually
    Duration ElapsedTimeInPeriod(TimePoint t) const;
    Duration ElapsedTimeInCurrentPeriod() const;

    TransmissionState ActionKindImpl(TimePoint t) const override;
    /// The next time for which ActionKindImpl will return a different result
    /// than it would return if provided the current time.
    TimePoint TimeOfNextActionImpl(TimePoint t) const override;

    Duration advertising_duration_;
    Duration response_wait_duration_;
    Duration sleep_duration_;
  };

public:
  enum class ConnectionGoal {
    kDisconnect,
    kSeekConnection,
    kAdvertiseConnection,
    kSeekAndAdvertiseConnection,
  };

  ProtocolAgent(Id id, RadioInterface &radio, MessagePipe pipe)
      : id_(id), radio_(radio), pipe_(pipe) {}

  void ExecuteAgentAction();

  void SetGoal(ConnectionGoal goal) { goal_ = goal; }

  bool InSession() { return (state_ == ProtocolState::kExecuteSession); }

private:
  enum LogLevel {
    kNone = 0,
    kLogTransitions,
    kLogPacketMetadata,
    kLogPacketBytes,
  };
  static constexpr LogLevel kLogLevel{kNone};

  static constexpr Duration kHandshakeLeadTime{std::chrono::milliseconds(100)};
  static constexpr auto kBaseAdvertisingInterval =
      std::chrono::milliseconds(550);
  // TODO implement dual seek/advertise mode
  // static constexpr auto kMaximumAdvertisingIntervalChange =
  // std::chrono::milliseconds(50);
  static constexpr auto kAdvertisingTransmissionDuration =
      std::chrono::milliseconds(200);
  // TODO set up scanning intervals within the non-advertising portion
  static constexpr auto kConnectionRequestInterval =
      kBaseAdvertisingInterval - kAdvertisingTransmissionDuration;
  static constexpr auto kHandshakeReceiveDuration =
      std::chrono::milliseconds(400);
  static constexpr auto kPendSleepTime = std::chrono::milliseconds(100);

  // TODO this is implicitly tied to the ToA computations I don't do yet
  static constexpr auto kHardcodedTransmissionTime =
      std::chrono::milliseconds(800);
  static constexpr auto kHardcodedSleepTime = std::chrono::milliseconds(200);

  void LogStr(const char* format, ...) const;
  void LogPacket(Packet const &p, [[maybe_unused]] WirePacket const &w_p,
                 const char *action) const;
  const char *StateStr(ProtocolState s) const;
  void ChangeState(ProtocolState new_state);

  std::pair<RadioInterface::Status, WirePacket> ReceivePacket();

  void DispatchNextState();
  void Pend();
  void Seek();
  void Advertise();
  void RequestConnection();
  void AcceptConnection();
  void ExecuteHandshakeFromSeek();
  void ExecuteSession();

  Id id_;

  std::reference_wrapper<RadioInterface> radio_;
  MessagePipe pipe_;
  std::optional<Session> session_;

  ProtocolState prior_state_{ProtocolState::kPend};
  std::atomic<ProtocolState> state_{ProtocolState::kDispatch};
  std::atomic<ConnectionGoal> goal_{ConnectionGoal::kDisconnect};
};

}; // namespace lora_chat
