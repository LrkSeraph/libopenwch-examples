# libopenwch examples

Working programs built on
[libopenwch](https://github.com/LrkSeraph/libopenwch), and the shared build
rules they use.  Three purposes: they document how each peripheral is driven,
they are what libopenwch's CI builds as an integration test, and they are code
to copy from.

**To start your own project use
[libopenwch-template](https://github.com/LrkSeraph/libopenwch-template)
instead** — an empty skeleton with libopenwch and the companion flasher as
submodules.  This repository is a collection of finished examples, not a
starting point.

## Quick start

```sh
# --recurse-submodules brings libopenwch with them
git clone --recurse-submodules \
    https://github.com/LrkSeraph/libopenwch-examples.git ~/src/libopenwch-examples
cd ~/src/libopenwch-examples/examples/blink
make            # the first make also builds the library
make flash      # needs a WCH-Link programmer
```

If you cloned without `--recurse-submodules`, `git submodule update --init`
fetches the library; `make OPENWCH_DIR=/path/to/libopenwch` builds against a
checkout somewhere else instead.

Seven examples ship here, and all build today:

| Example | Device | Family | What it does |
|---|---|---|---|
| `blink` | `ch32v003f4p6` | ch32v0 | 48 MHz from the internal RC oscillator, toggles PD1 |
| `uart_echo` | `ch32v003f4p6` | ch32v0 | USART1 echo at 115200 on PD5/PD6 |
| `uart_counter` | `ch32v003f4p6` | ch32v0 | USART1 transmit-only counter at 115200 on PD5, for `/dev/ttyACM0` |
| `sdi_counter` | `ch32v003f4p6` | ch32v0 | Counter over the WCH-LinkE SDI virtual serial port, using SWIO/GND/3.3 V only |
| `ch582_blink` | `ch582m` | ch5xx58x | 32 MHz crystal + PLL to 60 MHz, toggles PB4 |
| `ch582_uart_echo` | `ch582m` | ch5xx58x | UART1 echo at 115200 on PA8/PA9 |
| `ch582_ble_advertise` | `ch582m` | ch5xx58x | BLE peripheral: advertises as "libopenwch", LEDs on connect |

Each example targets one family, because the library archive is per family:
`blink` links only `libopenwch_ch32v0.a`, and building it with `DEVICE=ch582m`
is expected to fail at link time with undefined `rcc_*`/`gpio_*` references.
Within a family any sibling part works — `ch582_blink` builds unchanged for
`ch582m`, `ch583m`, `ch584m` and `ch585m`.

## Layout

```
libopenwch-examples/
├── libopenwch/              git submodule — the library
├── rules/
│   ├── toolchain.mk         toolchain discovery, programmer targets
│   └── rules.mk             compile/link/flash rules, OPENWCH_DIR lookup
├── examples/<name>/         main.c + Makefile, one directory per example
├── .clang-format  .vscode/  .github/workflows/
└── README.md  NOTICE  LICENSE
```

There is deliberately no top-level `Makefile`: each example is an independent
project, and the shared rules live in `rules/`.  All of them can still be built
from here:

```sh
for d in examples/*/; do make -C "$d" || exit 1; make -C "$d" clean; done
```

## The variables that matter

| Variable | Default | Meaning |
|---|---|---|
| `PROJECT` | — | basename of the output files (`blink` → `blink.elf`, `.bin`, `.hex`) |
| `DEVICE` | example specific | the part number, e.g. `ch32v003f4p6`.  Drives `-march`/`-mabi`, the linker script and which archive is linked |
| `OPENWCH_DIR` | submodule `./libopenwch` | where libopenwch is; guessed as submodule → `../libopenwch` → parent, each confirmed by `mk/genlink-config.mk` |
| `TEMPLATE_DIR` | `../..` from the example | this repository's root |
| `PREFIX` | auto-detected | toolchain prefix without the trailing `-` |
| `CFILES` | `main.c` | C sources, basenames only |
| `BUILD_DIR` | `bin` | object output directory |
| `OPT` / `CSTD` | `-Os` / `-std=c99` | optimisation and C standard |
| `INCLUDES` / `DEFS` / `CFLAGS` / `LDFLAGS` / `LDLIBS` | — | extra flags, appended |
| `PROGRAMMER` | `minichlink` | `minichlink` or `wchlink` |
| `MINICHLINK` / `WRITE_SECTION` | `minichlink` / `flash` | the flasher binary and its write region |
| `WCHLINK` | `wchlink` | the `wchlink` binary, used with `PROGRAMMER=wchlink` |
| `LIBOPENWCH_NOSTDLIB` | — | `1` links `-nostdlib` with the bundled mini-libc |
| `LIBOPENWCH_BLE` | `0` | `1` links WCH's Bluetooth stack (CH58x only) |

A new example is a directory in `examples/` with a `main.c` and a `Makefile` no
bigger than this:

```make
PROJECT     = my_example
DEVICE      = ch32v003f4p6
TEMPLATE_DIR ?= $(abspath ../..)
CFILES      = main.c
include $(TEMPLATE_DIR)/rules/rules.mk
```

## Toolchain

```sh
sudo apt-get install -y gcc-riscv64-unknown-elf
```

`rules/toolchain.mk` probes `riscv64-unknown-elf`, `riscv64-none-elf`,
`riscv32-unknown-elf`, `riscv-none-elf`, `riscv64-elf` and `riscv32-elf`;
override with `make PREFIX=/opt/xpack/bin/riscv-none-elf`.
`riscv64-linux-gnu-` is not probed: its conventions break bare-metal builds.

## Flashing

[minichlink](https://github.com/cnlohr/ch32fun) is the default: it drives the
WCH-Link and the built-in USB ISP bootloader and needs no vendor driver.

```sh
make flash                      # write the internal flash image
make monitor                    # printf over the single-wire debug channel
make unbrick                    # recover a part that stopped answering
make flash MINICHLINK=~/src/ch32fun/minichlink/minichlink
```

`PROGRAMMER=wchlink` uses
[libopenwch-tools](https://github.com/LrkSeraph/libopenwch-tools) instead,
taken from `PATH` unless `WCHLINK` names a binary.  These examples do not carry
that tool — libopenwch-template is the repository that wires it in as a
submodule.  The default stays `minichlink` because `wchlink` cannot flash yet;
it flips when it can.

## Output files

| File | Use |
|---|---|
| `<project>.elf` | debugging, `objdump`, `gdb` |
| `<project>.bin` | `make flash`, vendor flash tools |
| `<project>.hex` | the WCH official flash utility |
| `<project>.map` | link map |
| `<project>.list` | disassembly with source (`make blink.list`) |
| `generated.<device>.ld` | the linker script produced from `ld/devices.data` |

## Notes

* **`-nostartfiles` is mandatory**: libopenwch supplies its own `_start` and
  vector table, so the toolchain's crt0 must not be linked in.  The rule is
  already in `rules/rules.mk`.
* **Do not add `-march`/`-mabi` by hand.**  They come from `DEVICE` through
  `ld/devices.data`, so the application is always built for the same ISA as the
  library it links against; mixing `ilp32` and `ilp32e` silently corrupts the
  ABI.
* **`gpio_set_mode()` takes one opaque nibble**, not libopencm3's
  `(mode, cnf)` pair — see `include/libopenwch/ch32v0/common/gpio_common_v1.h`.
* **`LIBOPENWCH_BLE=1` links WCH's closed-source Bluetooth stack**, which is not
  part of `libopenwch_ch5xx58x.a` and is Apache-2.0 rather than LGPL.  It costs
  about 145 KB of flash and needs a heap the application declares with
  `BLE_HEAP_DEFINE`.  `ch582_ble_advertise` sets both switches itself.
