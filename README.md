# libopenwch examples

Working programs built on
[libopenwch](https://github.com/LrkSeraph/libopenwch), and the shared build
rules they use. They document each peripheral, act as the library's integration
test, and are code to copy from.

For a project skeleton, use
[libopenwch-template](https://github.com/LrkSeraph/libopenwch-template).

## Quick start

```sh
git clone --recurse-submodules \
    https://github.com/LrkSeraph/libopenwch-examples.git
cd libopenwch-examples/examples/blink
make            # builds the library too, if needed
make flash      # needs a WCH-Link programmer
```

Without `--recurse-submodules`, run `git submodule update --init`; or build
against another checkout with `make OPENWCH_DIR=/path/to/libopenwch`.

| Example | Device | Family | What it does |
|---|---|---|---|
| `blink` | `ch32v003f4p6` | ch32v0 | 48 MHz HSI PLL, blinks active-low LED on PC2 |
| `uart_echo` | `ch32v003f4p6` | ch32v0 | USART1 echo at 115200 on PD5/PD6 |
| `adc_dma_uart` | `ch32v003f4p6` | ch32v0 | ADC on PA1, continuous conversion + DMA to RAM, mean on USART1 |
| `ch582_blink` | `ch582m` | ch5xx58x | 32 MHz crystal + PLL to 60 MHz, toggles PB4 |
| `ch582_uart_echo` | `ch582m` | ch5xx58x | UART1 echo at 115200 on PA8/PA9 |
| `ch582_ble_advertise` | `ch582m` | ch5xx58x | BLE peripheral advertising as "libopenwch" |

Each example links one family archive; `DEVICE` can be any sibling part.

## Layout and variables

```
libopenwch/           libopenwch git submodule
rules/                toolchain discovery and build/flash rules
examples/<name>/      main.c + Makefile
```

Each example is independent; build all with:

```sh
for d in examples/*/; do make -C "$d" || exit 1; make -C "$d" clean; done
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
  `BLE_HEAP_DEFINE` heap; see `ch582_ble_advertise`.
