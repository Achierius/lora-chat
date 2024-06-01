#include <iostream>

#include <cassert>
#include <cstdio>

#include "config.hpp"
#include "lora_interface.hpp"
#include "user_interface.hpp"

int handle_user_command(UserCommand const &cmd) {
  switch (cmd.tag) {
  case UserCommandTag::kBadCommand:
    printf("failed to parse user command\n");
    return 0;
  case UserCommandTag::kTransmitMessage:
    printf("Transmitting message...");
    switch (lora_transmit(cmd.as_transmit_message.data(),
                          strlen(cmd.as_transmit_message.data()))) {
    case LoraStatus::kOk:
      printf("success\n");
      return 0;
    case LoraStatus::kUnspecifiedError:
      printf("unspecified radio error, dying");
      return -ENOTTY;
    case LoraStatus::kBadInput:
      printf("bad user input\n");
      return 0;
    }
  case UserCommandTag::kReceiveMessage:
  case UserCommandTag::kTransmitIota:
    printf("command not implemented :)\n");
    return 0;
  }
  return -1;
}

int main(int argc, char **args) {
  Config cfg{prompt_user_for_config()};

  assert((init_lora(cfg) == LoraStatus::kOk) && "Failed to initialize SPI or SP1276 radio");

  int return_value = 0;
  while (return_value == 0) {
    printf("  > ");
    UserCommand cmd{get_and_parse_user_input()};
    return_value = handle_user_command(cmd);
  }
  return return_value;
}
