# lora-chat
The software components of a two-way chat system built on top of LoRa radio.

## Hardware
This project uses the SX1276 LoRa Connect Transceiver by SemTech
([Link](https://www.semtech.com/products/wireless-rf/lora-connect/sx1276));
in particular, the pinouts are laid out for SparkFun's
[LoRa 1W Breakout - 915M30S](https://www.sparkfun.com/products/18572),
but if you change the pin maps it should work for any breakout.

## Building
First ensure that you have the [meson-build prereqs](https://mesonbuild.com/Quick-guide.html).
Then just run `meson setup build && meson compile -C build`.

## Running
Currently the only executable is `spi-repl`. Simple uses of this include reading
from SPI registers (input the address you want to read in hex) and writing to
SPI registers (input "0xADDR=0xVAL"), as well as transmitting values via LoRa radio
(input "%init-transmit", followed by "%transmit MSG").
