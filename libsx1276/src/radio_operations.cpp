#include "radio_operations.hpp"

#include <cassert>
#include <cstdio>
#include <mutex>
#include <utility>
#include <unordered_map>

#include "radio_math.hpp"
#include "spi_wrappers.hpp"

constexpr bool verbose { false };

// TODO move this to its own file?
namespace {
using ConfigCacheT = std::unordered_map<int, sx1276::ChannelConfig>;
ConfigCacheT* config_cache_storage;
ConfigCacheT& config_cache() {
  static std::once_flag once_flag;
  std::call_once(once_flag, []() {
  config_cache_storage = new ConfigCacheT();
  });
  return *config_cache_storage;
}

uint32_t compute_time_on_air_ms_via_fd(int msg_bytes, int fd) {
  assert(config_cache().count(fd) > 0);

  return compute_time_on_air_ms(msg_bytes, config_cache()[fd]);
}

} // namespace

bool sx1276::get_channel_config(int fd, ChannelConfig* config) {
  if (config_cache().count(fd) == 0) return false;

  *config = config_cache()[fd];
  return true;
}

void sx1276::init_lora(int fd, sx1276::ChannelConfig config) {
  using RegAddr = sx1276::RegAddr;
  using OpMode = sx1276::OpMode;

  if (config_cache().count(fd) > 0) {
    // For now we keep it to one initialization per fd per process
    printf("Error: multiple initializations for fd %d\n", fd);
    exit(-1);
  }

  auto& [freq, bw, cr, sf] = config;

  assert(sf >= 6 && sf <= 12);
  if (sf == 6) {
    printf("Error: SF6, while legal, is special & requires work I haven't done yet.\n");
    exit(-1);
  }
    
  auto check = [](std::pair<int, uint8_t> result) -> uint8_t {
    auto& [status, response] = result;
    if (status < 0) {
      perror("SPI_IOC_MESSAGE failed: ");
      exit(-1);
    }
    return response;
  };

  auto fence = [&](uint8_t addr) {
    // I don't know if this actually fences anything, but it's my best guess as
    // to what the RadioLib firmware is trying to achive by issuing a read after
    // every write it sends out
    check(spi_read_byte(fd, addr));
  };

  // First ensure that we're in LoRa mode
  {
    auto op_mode = check(spi_read_byte(fd, RegAddr::kOpMode));
    if ((op_mode & 0x80) == 0) {
      // We're in FSK/OOK mode, need to go into sleep to change over to LoRa
      check(spi_write_byte_masked(fd, RegAddr::kOpMode, OpMode::kSleep, 0x07));
      fence(RegAddr::kOpMode);
      // Now turn on LoRa mode
      check(spi_set_bit(fd, RegAddr::kOpMode, 7));
      fence(RegAddr::kOpMode);
      // And finally go back into standby
      check(spi_write_byte_masked(fd, RegAddr::kOpMode, OpMode::kStandby, 0x07));
      fence(RegAddr::kOpMode);
    }
  }
  // Now configure spooky hardware settings
  {
    // Errata says we need to turn off this bit after reset
    check(spi_unset_bit(fd, RegAddr::kDetectOptimize, 7));
    fence(RegAddr::kDetectOptimize);
    // Doing so resets the IfFreq registers, so we now reconfigure them
    // to the values that radiolib uses
    check(spi_write_byte(fd, RegAddr::kIfFreq1, 0x40));
    fence(RegAddr::kIfFreq1);
    check(spi_write_byte(fd, RegAddr::kIfFreq2, 0x00));
    fence(RegAddr::kIfFreq2);

    // Overload current protection
    check(spi_write_byte(fd, RegAddr::kOcp, 0x23));
    fence(RegAddr::kOcp);
    // Power limits
    // Unsure whether we need to set these one at a time but I'm not going to
    // start messing with the power settings now lol
    check(spi_set_bit(fd, RegAddr::kPaConfig, 7));
    fence(RegAddr::kPaConfig);
    check(spi_write_byte(fd, RegAddr::kPaConfig, 0xf8));
    fence(RegAddr::kPaConfig);
    // Use automatic gain control for LNA gain instead of manual control
    check(spi_write_byte(fd, RegAddr::kModemConfig3, 0x04));
    fence(RegAddr::kModemConfig3);
  }
  // Setup preamble length, sync word
  {
    // 0x12 seems to be standard, but anything other than 0x34 should be OK?
    check(spi_write_byte(fd, RegAddr::kSyncWord, sx1276::kSyncWordValue));
    fence(RegAddr::kSyncWord);
    check(spi_write_byte(fd, RegAddr::kPreambleMsb, (sx1276::kPreambleLengthBytes >> 8) && 0xFF));
    fence(RegAddr::kPreambleMsb);
    check(spi_write_byte(fd, RegAddr::kPreambleLsb, sx1276::kPreambleLengthBytes & 0xFF));
    fence(RegAddr::kPreambleLsb);
  }
  // Setup detection threashold/optimization
  {
    check(spi_write_byte_masked(fd, RegAddr::kDetectOptimize, 0x03, 0x07));
    fence(RegAddr::kDetectOptimize);
    check(spi_write_byte(fd, RegAddr::kDetectionThreshold, 0x0a));
    fence(RegAddr::kDetectionThreshold);
  }
  // Configure IQ inversions??
  {
    // bit 0 is TX invert, bit 6 is RX invert
    uint8_t iq_inversions = (0 << 6) | (1 << 0);  // Radiolib doesn't set bit 6
                                                  // And if I do it doesn't work
    uint8_t mask = (1 << 6) | (1 << 0);
    check(spi_write_byte_masked(fd, RegAddr::kInvertIq, iq_inversions, mask));
    fence(RegAddr::kInvertIq);
    // I don't really understand this next bit. Why do we not turn it on if we
    // ARE inverting IQ for TX? 0x1d is not inverted, 0x19 would be inverted.
    check(spi_write_byte(fd, RegAddr::kInvertIq2, 0x1d));
    fence(RegAddr::kInvertIq2);
  }
  // Setup the actual LoRa knobs
  {
    // Frequency
    uint8_t freq_msb = (freq >> 16) & 0xFF;
    uint8_t freq_mib = (freq >> 8) & 0xFF;
    uint8_t freq_lsb = freq & 0xFF;
    check(spi_write_byte(fd, RegAddr::kFreqMsb, freq_msb));
    fence(RegAddr::kFreqMsb);
    check(spi_write_byte(fd, RegAddr::kFreqMid, freq_mib));
    fence(RegAddr::kFreqMid);
    check(spi_write_byte(fd, RegAddr::kFreqLsb, freq_lsb));
    fence(RegAddr::kFreqLsb);

    // Bandwidth & coding rate (w/ explicit header mode)
    check(spi_write_byte(fd, RegAddr::kModemConfig1, (bw << 4) | (cr << 1)));
    fence(RegAddr::kModemConfig1);

    // Spreading factor & some other bits ig
    uint8_t rx_payload_crc = (kEnablePayloadCrc << 2);
    uint8_t up_rx_symb_timeout = 1;
    check(spi_write_byte(fd, RegAddr::kModemConfig2, (sf << 4) | rx_payload_crc | up_rx_symb_timeout));
    fence(RegAddr::kModemConfig2);
  }

  config_cache()[fd] = config;
}

void sx1276::lora_transmit(int fd, const uint8_t* msg, int len) {
  assert(len > 0);
  assert(len < 0xffff);
  assert(msg);

  using RegAddr = sx1276::RegAddr;

  // TODO error handle every single write :')

  spi_write_byte(fd, RegAddr::kOpMode, 0x89);
  spi_write_byte(fd, RegAddr::kPreambleMsb, 0x00);
  spi_write_byte(fd, RegAddr::kPreambleLsb, 0x08);
  spi_write_byte(fd, RegAddr::kHopPeriod, 0x00);

  spi_write_byte(fd, RegAddr::kPayloadLength, len);

  spi_write_byte(fd, RegAddr::kIrqFlags, 0xff);  // clear interrupts
  spi_write_byte(fd, RegAddr::kFifoTxBaseAddr, 0x80);
  spi_write_byte(fd, RegAddr::kFifoAddrPtr, 0x80);

  spi_write_burst(fd, RegAddr::kFifo, msg, len); // write msg to hw buffer
  spi_write_byte(fd, RegAddr::kOpMode, 0x8b); // begin transmitting

  auto time_on_air_us = compute_time_on_air_ms_via_fd(len, fd) * 1000;
  if (verbose)
    printf("lora_transmit: ToA  %u, now is %u\n", 150 + 7*len, time_on_air_us / 1000);
  usleep(time_on_air_us);
  spi_write_byte(fd, RegAddr::kOpMode, 0x89); // end transmitting
}

namespace {
bool lora_receive_common_setup(int fd) {
  using RegAddr = sx1276::RegAddr;

  // TODO error handle every single write :')

  spi_write_byte(fd, RegAddr::kOpMode, 0x89);
  spi_write_byte(fd, RegAddr::kPreambleMsb, 0x00);
  spi_write_byte(fd, RegAddr::kPreambleLsb, 0x08);
  spi_write_byte(fd, RegAddr::kHopPeriod, 0x00);

  spi_write_byte(fd, RegAddr::kFifoRxBaseAddr, 0x00);
  spi_write_byte(fd, RegAddr::kFifoAddrPtr, 0x00);
  spi_write_byte(fd, RegAddr::kIrqFlags, 0xff);  // clear interrupts

  return true;
}

bool copy_received_message(int fd, uint8_t* dest, int max_len) {
  assert(max_len >= 0);
  using RegAddr = sx1276::RegAddr;

  size_t payload_len { spi_read_byte(fd, RegAddr::kRxNumBytes).second };
  if (verbose)
    printf("received payload of length %lu: ", payload_len);
  if (payload_len > static_cast<size_t>(max_len)) {
    printf("warning: payload len %lu exceeded buffer size %d -- truncating\n", payload_len, max_len);
    payload_len = max_len;
  }

  auto [burst_status, burst_result] = spi_read_burst(fd, RegAddr::kFifo, payload_len);
  // TODO handle the case where we read too few bytes
  if (burst_status < 0) {
    printf("SPI burst-read failed: %s\n", strerror(-burst_status));
    return false;
  }

  // We ignore the first byte because it's duplicated for whatever reason
  std::memcpy(dest, burst_result.data() + 1, burst_result.size());

  return true;
}
} // namespace

bool sx1276::lora_receive_continuous(int fd, uint8_t* dest, int max_len) {
  assert(max_len);
  assert(dest);
  using RegAddr = sx1276::RegAddr;

  {
    auto result = lora_receive_common_setup(fd);
    if (!result) return result;
  }

  // If an RxDone interrupt is received in continuous mode, the chip enters an
  // unstable state (?) wherein any write to the IrqFlags register will drop the
  // whole chip into FSK FrequencySynthesis mode (OpMode 0x0c) and seemingly
  // lock it there until reset.
  // We get around this by masking off the RxDone interrupt and watching the
  // ValidHeader interrupt instead.
  // Unfortunately this means that we can't tell when the packet we received
  // was cut off midway -- TODO investigate further. If needed handle at the
  // protocol layer.
  uint8_t irq_mask { spi_read_byte(fd, RegAddr::kIrqFlagsMask).second };
  spi_write_byte(fd, RegAddr::kIrqFlagsMask, irq_mask | 0x40); // lol

  spi_write_byte(fd, RegAddr::kOpMode, 0x8d); // begin receiving
  auto time_on_air_us = compute_time_on_air_ms_via_fd(max_len, fd) * 1000;
  if (verbose)
    printf("lora_receive_continuous: ToA %ums\n", time_on_air_us / 1000);
  usleep(time_on_air_us);
  spi_write_byte(fd, RegAddr::kOpMode, 0x89); // stop receiving

  spi_write_byte(fd, RegAddr::kIrqFlagsMask, irq_mask); // restore prior state
  uint8_t irqs { spi_read_byte(fd, RegAddr::kIrqFlags).second };
  spi_write_byte(fd, RegAddr::kIrqFlags, 0x10 | 0x20);
  if (!(irqs & 0x10)) {
    return false;
  }
  // Check for CRC error
  if (irqs & 0x20) {
    printf("error: crc error detected\n");
    return false;
  }

  {
    auto result = copy_received_message(fd, dest, max_len);
    if (!result) return result;
  }

  return true;
}

bool sx1276::lora_receive_single(int fd, uint8_t* dest, int max_len) {
  assert(max_len);
  assert(dest);
  using RegAddr = sx1276::RegAddr;

  {
    auto result = lora_receive_common_setup(fd);
    if (!result) return result;
  }

  // TODO set timeout regs according to time_on_air_ms

  spi_write_byte(fd, RegAddr::kOpMode, 0x8e); // start transmitting
  auto time_on_air_us = compute_time_on_air_ms_via_fd(max_len, fd) * 1000;
  if (verbose)
    printf("lora_receive_single: ToA %ums\n", time_on_air_us / 1000);
  usleep(time_on_air_us);
  spi_write_byte(fd, RegAddr::kOpMode, 0x89); // start receiving

  // TODO what's wrong with my bitfield man
  //sx1276::IrqFlags irqs { spi_read_byte(fd, RegAddr::kIrqFlags).second };
  //if (!irqs.rx_done)

  uint8_t irqs { spi_read_byte(fd, RegAddr::kIrqFlags).second };
  spi_write_byte(fd, RegAddr::kIrqFlags, 0x40 | 0x20 | 0x10);
  if (!(irqs & 0x40)) {
    // TODO differentiate between a true radio-side timeout and our timeout
    // TODO check for valid header as well ?
    return false;
  }
  // Check for CRC error
  if (irqs & 0x20) {
    printf("error: crc error detected\n");
    return false;
  }

  {
    auto result = copy_received_message(fd, dest, max_len);
    if (!result) return result;
  }

  return true;
}
