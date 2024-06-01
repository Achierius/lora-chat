#include "user_interface.hpp"

#include <array>
#include <utility>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

UserCommand get_and_parse_user_input() {
  std::array<char, kMaxUserInputSize * 2> input_buffer{0};

  if (fgets(input_buffer.data(), sizeof(input_buffer), stdin) == NULL) {
    printf("error: empty input\n");
    return {.tag = UserCommandTag::kBadCommand};
  }
  input_buffer[strcspn(input_buffer.data(), "\n")] = '\0';

  // TODO handle special commands
  if (input_buffer[0] == '$') {
    // receive
    int wait_time_ms{0};
    sscanf(input_buffer.data() + 1, "%d", &wait_time_ms);
    return UserCommand{
        .as_receive_message = wait_time_ms,
        .tag = UserCommandTag::kReceiveMessage,
    };
  } else {
    // transmit
    UserCommand transmit_cmd{.tag = UserCommandTag::kTransmitMessage};
    std::memcpy(&transmit_cmd.as_transmit_message, input_buffer.data(),
                sizeof(transmit_cmd.as_transmit_message));
    return transmit_cmd;
  }
}
