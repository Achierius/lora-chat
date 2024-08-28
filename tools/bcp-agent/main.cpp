#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>

#include "bcp.hpp"

using namespace lora_chat;

static int kNextMessageId{0};

std::optional<WirePacketPayload> GetMessageToSend() {
  std::stringstream ss {};
  ss << "Ping " << kNextMessageId++;
  auto str = ss.str();
  WirePacketPayload p{};
  std::copy(str.begin(), str.end(), p.begin());
  return p;
}

void ConsumeMessage(WirePacketPayload &&payload) {
  printf("Message received \"%s\"\n",
      payload.data());
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("usage: %s <ID> <ACTION>; ACTION 0 to seek, 1 to advertise\n", argv[0]);
    return -1;
  }

  const WireSessionId id = std::stoi(argv[1]);
  const bool advertise = std::stoi(argv[2]);
  
  auto& radio = LoraInterface::instance();
  MessagePipe mpipe{GetMessageToSend, ConsumeMessage};

  ProtocolAgent agent{id, radio, mpipe};
  if (advertise)
    agent.SetGoal(ProtocolAgent::ConnectionGoal::kAdvertiseConnection);
  else
    agent.SetGoal(ProtocolAgent::ConnectionGoal::kSeekConnection);

  while (true) {
    agent.ExecuteAgentAction();
  }

  return 0;
}
