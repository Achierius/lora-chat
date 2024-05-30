#pragma once

#include <array>
#include <vector>
#include <unordered_set>
#include <utility>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

constexpr size_t kSpiMode = SPI_MODE_0;
constexpr size_t kSpiBits = 8;
constexpr size_t kSpiSpeed = 1000000;

inline int spi_init() {
  int fd = open("/dev/spidev0.0", O_RDWR);

  auto report_setup_failure_and_die = [](const char* ioctl, int status) {
    perror("failed: ");
    printf("%s failed: \n", ioctl);
    exit(-status);
  };

  if (fd < 0) report_setup_failure_and_die("OPEN", fd);

  int status = ioctl(fd, SPI_IOC_WR_MODE, &kSpiMode);
  if (status < 0) report_setup_failure_and_die("SPI_IOC_WR_MODE", status);
  status = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &kSpiBits);
  if (status < 0) report_setup_failure_and_die("SPI_IOC_WR_BITS_PER_WORD", status);
  status = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &kSpiSpeed);
  if (status < 0) report_setup_failure_and_die("SPI_IOC_WR_MAX_SPEED_HZ", status);

  return fd;
}

inline std::pair<int, uint8_t> spi_read_byte(int fd, uint8_t addr) {
  uint8_t tx[] = {addr, 0x00};
  uint8_t rx[2] = {0, 0};  // Response buffer
  struct spi_ioc_transfer tr = {
    .tx_buf = reinterpret_cast<unsigned long>(tx),
    .rx_buf = reinterpret_cast<unsigned long>(rx),
    .len = 2,
    .speed_hz = kSpiSpeed,
    .delay_usecs = 0,
    .bits_per_word = kSpiBits,
  };

  int status = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
  return {status, rx[1]};  // the value read out is in the second byte
}

inline std::pair<int, uint8_t> spi_write_byte(int fd, uint8_t addr, uint8_t val) {
  uint8_t tx[] = {(uint8_t)(addr | 0x80), val};
  uint8_t rx[2] = {0, 0};  // Response buffer
  struct spi_ioc_transfer tr = {
    .tx_buf = reinterpret_cast<unsigned long>(tx),
    .rx_buf = reinterpret_cast<unsigned long>(rx),
    .len = 2,
    .speed_hz = kSpiSpeed,
    .delay_usecs = 0,
    .bits_per_word = kSpiBits,
  };

  int status = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
  return {status, rx[1]};
}

inline std::pair<int, uint8_t> spi_write_byte_masked(int fd, uint8_t addr, uint8_t val, uint8_t mask) {
  auto [read_status, read_result] = spi_read_byte(fd, addr);
  if (read_status < 0) return {read_status, read_result};

  read_result &= ~mask;
  read_result |= val & mask;
  return spi_write_byte(fd, addr, read_result);
}

inline std::pair<int, uint8_t> spi_write_bit(int fd, uint8_t addr, bool val, int bit_idx) {
  assert(bit_idx < 8 && bit_idx > 0);
  return spi_write_byte_masked(fd, addr, static_cast<uint8_t>(val) << bit_idx, static_cast<uint8_t>(1 << bit_idx));
}

inline std::pair<int, uint8_t> spi_set_bit(int fd, uint8_t addr, int bit_idx) {
  return spi_write_bit(fd, addr, true, bit_idx);
}

inline std::pair<int, uint8_t> spi_unset_bit(int fd, uint8_t addr, int bit_idx) {
  return spi_write_bit(fd, addr, false, bit_idx);
}

inline std::pair<int, std::vector<uint8_t>> spi_read_burst(int fd, uint8_t addr, int len) {
  assert(len > 0);
  len += 1; // need space for the address byte

  std::vector<uint8_t> tx (len, '\0');
  std::vector<uint8_t> rx (len, '\0');

  tx[0] = addr;

  struct spi_ioc_transfer tr = {
    .tx_buf = reinterpret_cast<uint64_t>(tx.data()),
    .rx_buf = reinterpret_cast<uint64_t>(rx.data()),
    .len = static_cast<uint32_t>(len),
    .speed_hz = kSpiSpeed,
    .delay_usecs = 0,
    .bits_per_word = kSpiBits,
  };

  int status = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);

  return {status, rx};
}

inline std::pair<int, std::vector<uint8_t>> spi_write_burst(int fd, uint8_t addr, const uint8_t* data, int len) {
  assert(len > 0);
  len += 1; // need space for the address byte

  std::vector<uint8_t> tx (len, '\0');
  std::vector<uint8_t> rx (len, '\0');

  tx[0] = addr | 0x80;
  std::memcpy(tx.data() + 1, data, len - 1);

  struct spi_ioc_transfer tr = {
    .tx_buf = reinterpret_cast<uint64_t>(tx.data()),
    .rx_buf = reinterpret_cast<uint64_t>(rx.data()),
    .len = static_cast<uint32_t>(len),
    .speed_hz = kSpiSpeed,
    .delay_usecs = 0,
    .bits_per_word = kSpiBits,
  };

  int status = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);

  return {status, rx};
}
