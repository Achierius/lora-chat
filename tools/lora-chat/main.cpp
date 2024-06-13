#include <iostream>

#include <cassert>
#include <cstdio>

#include "config.hpp"
#include "lora_interface.hpp"
#include "user_interface.hpp"

int handle_transmit_message(TransmitMessagePayload message) {
  printf("Transmitting message... ");
  switch (lora_transmit(message.data(),
                        strlen(message.data()))) {
  case TransmitStatus::kSuccess:
    printf("success\n");
    return 0;
  case TransmitStatus::kUnspecifiedError:
    printf("unspecified radio error, dying");
    return -ENOTTY;
  case TransmitStatus::kBadInput:
    printf("bad user input\n");
    return 0;
  }
  return -1;
}

int handle_receive_message(ReceiveMessagePayload payload) {
  printf("Receiving %d messages... \n", payload.first);
  for (int i = 0; i < payload.first; i++) {
    auto [status, msg] = lora_receive(payload.second);
    switch (status) {
    case ReceiveStatus::kSuccess:
      printf("\"%s\"\n", msg->c_str());
      break;
    case ReceiveStatus::kNoMessage:
      printf("timed out\n");
      break;
    case ReceiveStatus::kUnspecifiedError:
      printf("unspecified radio error, dying");
      return -ENOTTY;
    case ReceiveStatus::kBadInput:
      printf("bad user input\n");
      return 0;
    }
  }
  printf("... complete.\n");
  return 0;
}


int handle_user_command(UserCommand const &cmd) {
  switch (cmd.tag) {
  case UserCommandTag::kBadCommand:
    printf("failed to parse user command\n");
    return 0;
  case UserCommandTag::kTransmitMessage:
    return handle_transmit_message(cmd.as_transmit_message);
  case UserCommandTag::kReceiveMessage:
    return handle_receive_message(cmd.as_receive_message);
  case UserCommandTag::kTransmitIota:
    printf("command not implemented :)\n");
    return 0;
  }
  return -1;
}

int main(int argc, char **args) {
  Config cfg{prompt_user_for_config()};

  assert(init_lora(cfg) && "Failed to initialize SPI or SP1276 radio");

  int return_value = 0;
  while (return_value == 0) {
    printf("  > ");
    UserCommand cmd{get_and_parse_user_input()};
    return_value = handle_user_command(cmd);
  }
  return return_value;
}
