#include "radio_operations.hpp"

#include <utility>

#include <cassert>
#include <cstdio>

void Sx127x::init_lora(int fd, uint32_t freq, Sx127x::Bandwidth bw,
    Sx127x::CodingRate cr, int spreading_factor) {
  using RegAddr = Sx127x::RegAddr;
  using OpMode = Sx127x::OpMode;
  using Bandwidth = Sx127x::Bandwidth;
  using CodingRate = Sx127x::CodingRate;

  assert(spreading_factor >= 6 && spreading_factor <= 12);
  if (spreading_factor == 6) {
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
    check(spi_write_byte(fd, RegAddr::kSyncWord, 0x12));
    fence(RegAddr::kSyncWord);
    check(spi_write_byte(fd, RegAddr::kPreambleMsb, 0x00));
    fence(RegAddr::kPreambleMsb);
    check(spi_write_byte(fd, RegAddr::kPreambleLsb, 0x08));
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
    uint8_t iq_inversions = (1 << 6) | (1 << 0);  // Radiolib doesn't set bit 6
                                                  // when just transmitting, idk
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
    uint8_t rx_payload_crc = (1 << 2);
    check(spi_write_byte(fd, RegAddr::kModemConfig2, (spreading_factor << 4) | rx_payload_crc));
    fence(RegAddr::kModemConfig2);
  }
}

void Sx127x::lora_transmit(int fd, int time_on_air_ms, const uint8_t* msg, int len) {
  assert(len > 0);
  assert(len < 0xffff);
  assert(msg);

  using RegAddr = Sx127x::RegAddr;

  // TODO compute TOA from message length
  uint64_t time_on_air_us = time_on_air_ms * 1000;

  // TODO error handle every single write :')

  spi_write_byte(fd, RegAddr::kOpMode, 0x89);
  spi_write_byte(fd, RegAddr::kPreambleMsb, 0x00);
  spi_write_byte(fd, RegAddr::kPreambleLsb, 0x08);
  spi_write_byte(fd, RegAddr::kHopPeriod, 0x00);

  spi_write_byte(fd, RegAddr::kPayloadLength, len);

  spi_write_byte(fd, RegAddr::kIrqFlags, 0xff);  // clear interrupts
  spi_write_byte(fd, RegAddr::kFifoTxBaseAddr, 0x00);
  spi_write_byte(fd, RegAddr::kFifoAddrPtr, 0x00);

  spi_write_burst(fd, 0x00, msg, len);
  spi_write_byte(fd, 0x01, 0x8b);
  usleep(time_on_air_us);
  spi_write_byte(fd, 0x01, 0x89);
}
