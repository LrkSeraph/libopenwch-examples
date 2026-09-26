# libopenwch examples

Working programs built on
[libopenwch](https://github.com/LrkSeraph/libopenwch), grouped by chip family.
They document each peripheral, act as the library's integration test, and are
code to copy from.

For a project skeleton, use
[libopenwch-template](https://github.com/LrkSeraph/libopenwch-template).

## Quick start

```sh
git clone --recurse-submodules \
    https://github.com/LrkSeraph/libopenwch-examples.git
cd libopenwch-examples/examples/ch32v0/blink
make            # builds the library too, if needed
make flash      # needs a WCH-Link programmer
```

Without `--recurse-submodules`, run `git submodule update --init`; or build
against another checkout with `make OPENWCH_DIR=/path/to/libopenwch`.

After the `libopenwch` submodule is updated, rebuild its archives once:

```sh
make -C libopenwch
```

The per-example rule only builds the archive when it is missing, so a source
update alone does not upgrade an already-built `libopenwch_*.a`.

| Family | Example | Device | What it does |
|---|---|---|---|
| ch32v0 | `adc_dma_uart` | `ch32v003f4p6` | PA1 ADC continuous, DMA to RAM, mean on USART1 |
| ch32v0 | `blink` | `ch32v003f4p6` | 48 MHz HSI PLL, active-low LED on PC2 |
| ch32v0 | `selftest/dma_selftest` | `ch32v003f4p6` | DMA1 channel 1 SRAM-to-SRAM 8/32-bit transfers; PASS/FAIL and measured throughput on USART1 |
| ch32v0 | `spi_nor_crc` | `ch32v003f4p6` | SPI1 PC4/PC5/PC6/PC7, detects NOR flash, prints ID/capacity/SFDP, then CRC32 per 4 KiB page |
| ch32v0 | `selftest/timer_selftest` | `ch32v003f4p6` | TIM2 1 Hz update; SysTick-measured period and PASS/WARN on USART1 |
| ch32v0 | `uart_counter` | `ch32v003f4p6` | prints chip id/sysclk, then a counter per second on USART1 PD5 |
| ch5xx58x | `ch582_ble_advertise` | `ch582m` | BLE peripheral advertising as "libopenwch" |
| ch5xx58x | `ch582_blink` | `ch582m` | 32 MHz crystal + PLL to 60 MHz, LED on PB4 |
| ch5xx58x | `ch582_uart_counter` | `ch582m` | prints chip id/sysclk, then a counter per second on UART1 PA8 |

`spi_nor_crc` checksums the first 16 pages (64 KiB) by default; override with
`make CFLAGS+=-DFLASH_PAGE_COUNT=N`.  Each example links one family archive;
`DEVICE` can be any sibling part.

## Layout and variables

```
libopenwch/                 libopenwch git submodule
rules/                      toolchain discovery and build/flash rules
examples/<family>/<name>/   main.c + Makefile
examples/ch32v0/selftest/   grouped DMA and timer self-tests
```

Each example is independent; build all with:

```sh
for d in examples/*/*/; do make -C "$d" || exit 1; make -C "$d" clean; done
```

Important variables: `PROJECT`, `DEVICE`, `OPENWCH_DIR`, `PREFIX`,
`CFILES`, `CFLAGS`, `LIBOPENWCH_NOSTDLIB`, `LIBOPENWCH_BLE`, `PROGRAMMER`,
`WCHLINK`, `MINICHLINK`.

## Flash and output

`make flash` defaults to [minichlink](https://github.com/cnlohr/ch32fun);
`PROGRAMMER=wchlink` uses
[libopenwch-tools](https://github.com/LrkSeraph/libopenwch-tools) from `PATH`
or `WCHLINK`. The examples do not carry that tool.

| File | Use |
|---|---|
| `<project>.elf` | debug, `objdump`, GDB |
| `<project>.bin` | `make flash` |
| `<project>.hex` | WCH's official utility |
| `<project>.map`, `.list` | link map and disassembly |

Notes:

* `-nostartfiles` is mandatory; libopenwch owns `_start` and the vector table.
* Do not add `-march`/`-mabi` by hand; they come from `DEVICE`.
* CH32V00x `gpio_set_mode()` takes WCH's opaque `GPIO_Mode_*` token.
* `LIBOPENWCH_BLE=1` links WCH's Apache-2.0 BLE stack and needs a
  `BLE_HEAP_DEFINE` heap; see `ch5xx58x/ch582_ble_advertise`.
