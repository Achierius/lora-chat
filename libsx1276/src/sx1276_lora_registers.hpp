#pragma once

#define SX127x_REG_FIFO 0x00
#define SX127x_REG_OP_MODE 0x01
#define SX127x_REG_FREQ_MSB 0x06
#define SX127x_REG_FREQ_MID 0x07
#define SX127x_REG_FREQ_LSB 0x08
#define SX127x_REG_PA_CONFIG 0x09
#define SX127x_REG_PA_RAMP 0x0A
#define SX127x_REG_OCP 0x0B
#define SX127x_REG_LNA 0x0C
#define SX127x_REG_FIFO_ADDR_PTR 0x0D
#define SX127x_REG_FIFO_TX_BASE_ADDR 0x0E
#define SX127x_REG_FIFO_RX_BASE_ADDR 0x0F
#define SX127x_REG_FIFO_RX_CURRENT_ADDR 0x10
#define SX127x_REG_IRQ_FLAGS_MASK 0x11
#define SX127x_REG_IRQ_FLAGS 0x12
#define SX127x_REG_RX_NUM_BYTES 0x13
#define SX127x_REG_RX_HEADER_COUNT_VALUE_MSB 0x14
#define SX127x_REG_RX_HEADER_COUNT_VALUE_LSB 0x15
#define SX127x_REG_RX_PACKET_COUNT_VALUE_MSB 0x16
#define SX127x_REG_RX_PACKET_COUNT_VALUE_LSB 0x17
#define SX127x_REG_MODEM_STAT 0x18
#define SX127x_REG_PKT_SNR_VALUE 0x19
#define SX127x_REG_PKT_RSSI_VALUE 0x1A
#define SX127x_REG_RSSI_VALUE 0x1B
#define SX127x_REG_HOP_CHANNEL 0x1C
#define SX127x_REG_MODEM_CONFIG1 0x1D
#define SX127x_REG_MODEM_CONFIG2 0x1E
#define SX127x_REG_SYMB_TIMEOUT_LSB 0x1F
#define SX127x_REG_PREAMBLE_MSB 0x20
#define SX127x_REG_PREAMBLE_LSB 0x21
#define SX127x_REG_PAYLOAD_LENGTH 0x22
#define SX127x_REG_MAX_PAYLOAD_LENGTH 0x23
#define SX127x_REG_HOP_PERIOD 0x24
#define SX127x_REG_FIFO_RX_BYTE_ADDR 0x25
#define SX127x_REG_MODEM_CONFIG3 0x26
#define SX127x_REG_FEI_MSB 0x28
#define SX127x_REG_FEI_MID 0x29
#define SX127x_REG_FEI_LSB 0x2A
#define SX127x_REG_RSSI_WIDEBAND 0x2C
#define SX127x_REG_DETECT_OPTIMIZE 0x31
#define SX127x_REG_INVERT_IQ 0x33
#define SX127x_REG_HBW_OPTIMIZE1 0x36
#define SX127x_REG_DETECTION_THRESHOLD 0x37
#define SX127x_REG_SYNC_WORD 0x39
#define SX127x_REG_HBW_OPTIMIZE2 0x3A
#define SX127x_REG_INVERT_IQ2 0x3B

#ifdef __cplusplus
namespace Sx127x {
#endif // __cplusplus

enum RegAddr : uint8_t {
  kFifo = 0x00,
  kOpMode = 0x01,
  kFreqMsb = 0x06,
  kFreqMid = 0x07,
  kFreqLsb = 0x08,
  kPaConfig = 0x09,
  kPaRamp = 0x0A,
  kOcp = 0x0B,
  kLna = 0x0C,
  kFifoAddrPtr = 0x0D,
  kFifoTxBaseAddr = 0x0E,
  kFifoRxBaseAddr = 0x0F,
  kFifoRxCurrentAddr = 0x10,
  kIrqFlagsMask = 0x11,
  kIrqFlags = 0x12,
  kRxNumBytes = 0x13,
  kRxHeaderCountValueMsb = 0x14,
  kRxHeaderCountValueLsb = 0x15,
  kRxPacketCountValueMsb = 0x16,
  kRxPacketCountValueLsb = 0x17,
  kModemStat = 0x18,
  kPktSnrValue = 0x19,
  kPktRssiValue = 0x1A,
  kRssiValue = 0x1B,
  kHopChannel = 0x1C,
  kModemConfig1 = 0x1D,
  kModemConfig2 = 0x1E,
  kSymbTimeoutLsb = 0x1F,
  kPreambleMsb = 0x20,
  kPreambleLsb = 0x21,
  kPayloadLength = 0x22,
  kMaxPayloadLength = 0x23,
  kHopPeriod = 0x24,
  kFifoRxByteAddr = 0x25,
  kModemConfig3 = 0x26,
  kFeiMsb = 0x28,
  kFeiMid = 0x29,
  kFeiLsb = 0x2A,
  kRssiWideband = 0x2C,
  kDetectOptimize = 0x31,
  kInvertIq = 0x33,
  kHbwOptimize1 = 0x36,
  kDetectionThreshold = 0x37,
  kSyncWord = 0x39,
  kHbwOptimize2 = 0x3A,
  kInvertIq2 = 0x3B,
};

enum OpMode : uint8_t {
  kSleep = 0x0,
  kStandby = 0x1,
  kFrequencySynthesisTransmit = 0x2,
  kTransmit = 0x3,
  kFrequencySynthesisReceive = 0x4,
  kReceiveContinuous = 0x5,
  kReceiveSingle = 0x6,
  kChannelActivityDetection = 0x7,
};

enum Bandwidth : uint8_t {
  k7_8kHz = 0,
  k10_4kHz = 1,
  k15_6kHz = 2,
  k20_8kHz = 3,
  k31_25kHz = 4,
  k41_7kHz = 5,
  k62_5kHz = 6,
  k125kHz = 7,
  k250kHz = 8,
  k500kHz = 9,
};

enum CodingRate : uint8_t {
  kUndefinedCodingRate = 0,
  k4_5 = 1,
  k4_6 = 2,
  k4_7 = 3,
  k4_8 = 4,
};

#ifdef __cplusplus
} // namespace Sx127x
#endif // __cplusplus

