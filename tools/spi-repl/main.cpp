#include <array>
#include <unordered_set>
#include <utility>
#include <vector>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "sx1276/sx1276.hpp"

constexpr size_t kFifoMaxCapacity = 66;
constexpr size_t kMaxSpiAddress = 0x70;

const std::unordered_set<uint8_t> kSpiAddressGaps {
  0x43, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4c,
  0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
  0x56, 0x57, 0x58, 0x59, 0x5a, 0x5c, 0x5e, 0x5f,
  0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c,
  0x6d, 0x6e, 0x6f
};


void read_from_spi_cmd(int fd, uint8_t addr) {
  printf("Reading from register 0x%02x: ", addr);
  auto [status, response] = spi_read_byte(fd, addr);
  if (status < 0) {
    perror("SPI_IOC_MESSAGE failed: ");
  } else {
    printf("success (%d): 0x%02x\n", status, response);
  }
}

void write_to_spi_cmd(int fd, uint8_t addr, uint8_t val) {
  printf("Writing 0x%02x to register 0x%02x: ", val, addr);
  auto [status, response] = spi_write_byte(fd, addr, val);
  if (status < 0) {
    perror("SPI_IOC_MESSAGE failed: ");
  } else {
    printf("success (%d): 0x%02x\n", status, response);
  }
}

void burst_read_from_spi_cmd(int fd, uint8_t addr, int len) {
  printf("Burst-reading %d bytes starting from 0x%02x: ", len, addr);
  auto [status, response] = spi_read_burst(fd, addr, len);
  if (status < 0) {
    perror("SPI_IOC_MESSAGE failed: ");
  } else {
    printf("success (%d): ", status);
    bool skip_first = true;
    for (auto& b : response) {
      if (skip_first) {
        skip_first = false;
        continue;
      }
      printf("0x%02x ", b);
    }
    printf("\n");
  }
}

void diff_cmd(int fd) {
  printf("Recording SPI values... ");
  std::array<uint8_t, kMaxSpiAddress + 1> diff_buff{0};
  for (uint8_t addr = 0; addr <= kMaxSpiAddress; addr++) {
    if (kSpiAddressGaps.count(addr) > 0) continue;
    auto [status, val] = spi_read_byte(fd, addr);
    if (status < 0) {
      perror("SPI_IOC_MESSAGE failed: ");
      printf("error: Diff failed while reading 0x%02x\n", addr);
      return;
    }
    diff_buff[addr] = val;
  }

  printf("complete! Press enter to check diff...");
  static_cast<void>(getchar());
  int delta_count = 0;
  for (uint8_t addr = 0; addr <= kMaxSpiAddress; addr++) {
    if (kSpiAddressGaps.count(addr) > 0) continue;
    auto [status, val] = spi_read_byte(fd, addr);
    if (status < 0) {
      perror("SPI_IOC_MESSAGE failed: ");
      printf("error: Diff failed while reading 0x%02x\n", addr);
      return;
    }
    if (diff_buff[addr] != val) {
      printf(" * 0x%02x: was 0x%02x, is 0x%02x\n", addr, diff_buff[addr], val);
      delta_count++;
    }
  }
  printf("Diff complete: %d deltas\n", delta_count);
}

int main() {

  // Configuration and data transfer code goes here
  printf("Initializing SPI... ");
  int fd = spi_init();
  printf("success!\nEnter SPI register address to read (e.g. 0x20):\n");

  constexpr size_t kBuffSize = 256;
  char input_buffer[kBuffSize];
  std::array<uint8_t, kMaxSpiAddress + 1> last_diff_state {0};
  while (!feof(stdin)) {
    unsigned addr = 0;
    unsigned val = 0;

    printf("Enter command: ");

    memset(&input_buffer, 0, sizeof(input_buffer));
    if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
      printf("error: empty input\n");
      continue;
    }
    input_buffer[strcspn(input_buffer, "\n")] = '\0';

    if (input_buffer[0] == '%') {
      // execute special command
      auto match_n = [&](const char* candidate, size_t n) -> bool {
        assert(n <= kBuffSize);
        // skip the leading %
        return (strncmp(input_buffer + 1, candidate, n) == 0);
      };
      auto match = [&](const char* candidate) -> bool {
        return match_n(candidate, kBuffSize);
      };

      if (match("diff")) {
        diff_cmd(fd);
      } else if (match_n("burst ", 6)) {
        sscanf(input_buffer + 7, "%x %d", &addr, &val);
        if (addr > kMaxSpiAddress || addr < 0) {
          printf("Address 0x%02x is greater than the maximum (0x%02x), please select "
              "a new register\n", addr, kMaxSpiAddress);
          continue;
        }
        if (val < 1) {
          printf("Burst length %d must be at least 1\n", val);
          continue;
        }
        burst_read_from_spi_cmd(fd, addr, val);
      } else {
        printf("error: unknown command '%s'\n", input_buffer+1);
      }
      continue;
    }

    int match_count = sscanf(input_buffer, "%x=%x", &addr, &val);
    if (addr > kMaxSpiAddress || addr < 0) {
      printf("Address 0x%02x is greater than the maximum (0x%02x), please select "
          "a new register\n", addr, kMaxSpiAddress);
      continue;
    }
    if (val > 0xffff || val < 0) {
      printf("Invalid value 0x%x, please try again\n", val);
      continue;
    }
    if (match_count == 2) write_to_spi_cmd(fd, addr, val);
    else if (match_count == 1) read_from_spi_cmd(fd, addr);
    else printf("error while processing input \"%s\"\n", &input_buffer);
  }

  close(fd);
  return 0;
}
