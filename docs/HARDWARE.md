# Hardware and wiring

- LUCKFOX ESP32-P4-POE-ETH-NH / Waveshare ESP32-P4-ETH with IP101 PHY.
- Target chip: ESP32-P4 revision 3.2, 32 MB flash.
- SparkFun MAX-M10S GNSS Receiver Breakout (GPS-18037), 3.3 V, with SMA antenna.
- GME25664-65, 2.08-inch SH1122 OLED, 256x64, 7-pin SPI.
- Custom printed insert for the **DAP MP-1 2U modular rack frame**, occupying two
  of its ten segments. This is an insert, not a complete 19-inch rack panel.

The [Enclosure files and printing notes](../hardware/dap-mp1-2u/README.md)
include a front panel with three assignable filament regions and an open-back cover.

| Connection | ESP32-P4 GPIO |
|---|---:|
| GNSS TX to ESP RX | 21 |
| ESP TX to GNSS RX | 22 |
| GNSS PPS | 20 |
| OLED SCK | 14 |
| OLED MOSI | 15 |
| OLED reset | 16 |
| OLED DC | 17 |
| OLED CS | 18 |

Use a common ground and 3.3 V for GNSS and OLED. On this PoE hardware, do not power
through USB and PoE simultaneously. Verify wiring before using a different board.
