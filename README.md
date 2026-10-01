# libopenwch examples

Working programs for
[libopenwch](https://github.com/LrkSeraph/libopenwch), grouped by family. They
document peripherals, double as integration tests, and are meant to be copied.
For a project skeleton use
[libopenwch-template](https://github.com/LrkSeraph/libopenwch-template).

## Quick start

```sh
git clone --recurse-submodules https://github.com/LrkSeraph/libopenwch-examples.git
cd libopenwch-examples/examples/ch32v0/blink
make            # builds libopenwch too if needed
make flash      # WCH-Link + minichlink by default
```

Without `--recurse-submodules`, run `git submodule update --init`; or build
against another checkout with `make OPENWCH_DIR=/path/to/libopenwch`. After a
submodule source update, rebuild the archive once with `make -C libopenwch`.

| Family | Example | Device | Purpose |
|---|---|---|---|
| ch32v0 | `adc_dma_uart` | ch32v003f4p6 | PA1 ADC + DMA + mean on USART1 |
| ch32v0 | `blink` | ch32v003f4p6 | 48 MHz HSI-PLL, active-low PC2 LED |
| ch32v0 | `pwm` | ch32v003f4p6 | TIM2_CH2/PC2 1 kHz, raised-cosine breathing |
| ch32v0 | `ssd1315_i2c` | ch32v003f4p6 | SSD1315 128x64 OLED on I2C1, selectable default/partial/full remap |
| ch32v0 | `selftest` | ch32v003f4p6 | integrated RCC/SysTick/GPIO/USART/DMA/TIM/ADC/SPI/I2C/ESIG/EXTEN + IWDG/WWDG reset |
| ch32v0 | `spi_nor_crc` | ch32v003f4p6 | SPI1 NOR detect, ID/capacity/SFDP, 4 KiB CRC32 |
| ch32v0 | `uart_counter` | ch32v003f4p6 | chip id/sysclk + counter per second |
| ch5xx58x | `ch582_ble_advertise` | ch582m | BLE peripheral "libopenwch" |
| ch5xx58x | `ch582_blink` | ch582m | 32 MHz crystal + PLL to 60 MHz, PB4 LED |
| ch5xx58x | `ch582_uart_counter` | ch582m | chip id/sysclk + counter on UART1 PA8 |

`spi_nor_crc` checks first 16 pages (64 KiB); override with
`make CFLAGS+=-DFLASH_PAGE_COUNT=N`. The SSD1315 example defaults to full
remap `PC5/PC6`; use `SSD1315_I2C_PINS=0` for default `PC1/PC2` or `1` for
partial `PD0/PD1`. Build all:

```sh
for d in examples/*/*/; do make -C "$d" || exit 1; done
```

## Layout and variables

```text
libopenwch/                 submodule
rules/                      toolchain/build/flash rules
examples/<family>/<name>/   main.c + Makefile
examples/ch32v0/selftest/   integrated self-test
```

Key variables: `PROJECT`, `DEVICE`, `OPENWCH_DIR`, `PREFIX`, `CFILES`,
`CFLAGS`, `LIBOPENWCH_NOSTDLIB`, `LIBOPENWCH_BLE`, `PROGRAMMER`, `WCHLINK`,
`MINICHLINK`.

## Flash and output

`make flash` defaults to [minichlink](https://github.com/cnlohr/ch32fun);
`PROGRAMMER=wchlink` uses [libopenwch-tools](https://github.com/LrkSeraph/libopenwch-tools)
from `PATH` or `WCHLINK`.

| File | Use |
|---|---|
| `<project>.elf` | debug, objdump, GDB |
| `<project>.bin` | `make flash` |
| `<project>.hex` | WCH utility |
| `.map`, `.list` | link map / disassembly |

Notes: `-nostartfiles` is mandatory; do not add `-march`/`-mabi` by hand;
CH32V00x `gpio_set_mode()` uses opaque `GPIO_Mode_*` tokens;
`LIBOPENWCH_BLE=1` links WCH's Apache-2.0 stack and needs `BLE_HEAP_DEFINE`.
