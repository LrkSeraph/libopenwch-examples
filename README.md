# libopenwch examples

Working programs built on
[libopenwch](https://github.com/LrkSeraph/libopenwch), and the shared build
rules they use.  They serve three purposes: they document how each peripheral
is meant to be driven, they are what libopenwch's CI builds as an integration
test, and they are a source of code to copy from.

**To start your own project, use
[libopenwch-template](https://github.com/LrkSeraph/libopenwch-template)
instead** — that is an empty project skeleton with libopenwch as a submodule.
This repository is a collection of finished examples; it is not a starting
point, and the examples are not meant to be edited into a product.

Both this repository and `libopenwch-template` carry libopenwch as a git
submodule rather than vendoring it: the library is something you build
*against*, and a submodule makes updating it `git submodule update --remote`
rather than a merge.

## Quick start

```sh
# 1. Clone the examples; --recurse-submodules brings libopenwch with them.
git clone --recurse-submodules \
    https://github.com/LrkSeraph/libopenwch-examples.git ~/src/libopenwch-examples
cd ~/src/libopenwch-examples

# 2. Build one.  The first make also builds libopenwch.
cd examples/blink
make

# 3. Flash it (needs a WCH-Link programmer, see below)
make flash
```

If you cloned without `--recurse-submodules`, `git submodule update --init`
fetches the library, and `make OPENWCH_DIR=/path/to/libopenwch` builds against
a checkout somewhere else instead.

`OPENWCH_DIR` is only guessed when you have not set it.  The guess tries the
submodule `./libopenwch`, then `../libopenwch` next to this checkout, then the
parent directory, confirming each by looking for `mk/genlink-config.mk`; if none
matches, the build stops and names the paths it tried.  Setting `OPENWCH_DIR` —
on the command line, in the environment, or in your own `Makefile` — always
wins.

Five examples ship here and all build today:

| Example | Device | Family | What it does |
|---|---|---|---|
| `examples/blink` | `ch32v003f4p6` | ch32v0 | 48 MHz from the internal RC oscillator, toggles PD1 |
| `examples/uart_echo` | `ch32v003f4p6` | ch32v0 | USART1 echo at 115200 on PD5/PD6 |
| `examples/ch582_blink` | `ch582m` | ch5xx58x | 32 MHz crystal + PLL to 60 MHz, toggles PB4 |
| `examples/ch582_uart_echo` | `ch582m` | ch5xx58x | UART1 echo at 115200 on PA8/PA9 |
| `examples/ch582_ble_advertise` | `ch582m` | ch5xx58x | Bluetooth LE peripheral: advertises as "libopenwch", LEDs on connect |

Each example targets one family, because the library archive is per family:
`blink` uses the CH32V00x `rcc`/`gpio` drivers and therefore only links against
`libopenwch_ch32v0.a`.  Building it with `DEVICE=ch582m` is expected to fail at
link time with undefined `rcc_*`/`gpio_*` references.  Within a family, though,
any sibling part works — `examples/ch582_blink` builds unchanged for
`ch582m`, `ch583m`, `ch584m` and `ch585m`, and `examples/blink` for every
CH32V00x part.

## Starting your own project

Not from here — use
[libopenwch-template](https://github.com/LrkSeraph/libopenwch-template), which
is an empty project with libopenwch already wired in as a submodule.  These
examples are references: read one, copy the parts you need, or copy a whole
example into your own project and adapt it.

```sh
cp -r ~/src/libopenwch-examples/examples/blink ~/src/my-firmware/src
```

`main.c` only has to provide `int main(void)`.  Everything else — the reset
path, `.data`/`.bss` initialisation, the vector table and the entry point —
comes from libopenwch's QingKe core layer.

## Layout

```
libopenwch-examples/
├── README.md  NOTICE  LICENSE
├── .clang-format            house style, shared with libopenwch
├── .gitignore
├── .vscode/                 editor configuration (IntelliSense, debug, tasks)
├── libopenwch/              git submodule — the library
├── rules/
│   ├── toolchain.mk         toolchain discovery, programmer targets
│   └── rules.mk             compile/link/flash rules, OPENWCH_DIR lookup
└── examples/
    ├── blink/               CH32V003
    ├── uart_echo/           CH32V003
    ├── ch582_blink/         CH58x core-layer bring-up
    ├── ch582_uart_echo/     CH58x
    └── ch582_ble_advertise/ CH58x, Bluetooth LE
```

There is deliberately no top-level `Makefile`: each example is an independent
project with its own, and the shared rules live in `rules/`.  A top-level
`make` can still drive them all:

```sh
# build every example
for d in examples/*/; do make -C "$d" || exit 1; done
```

## The variables that matter

| Variable | Default | Meaning |
|---|---|---|
| `PROJECT` | — | basename of the output files (`blink` → `blink.elf`, `blink.bin`, `blink.hex`) |
| `DEVICE` | example specific | the part number, e.g. `ch32v003f4p6`.  Drives `-march`/`-mabi`, the linker script and which library is linked |
| `OPENWCH_DIR` | submodule `./libopenwch` | path to the libopenwch checkout |
| `TEMPLATE_DIR` | `../..` from the example | path to this repository's root |
| `PREFIX` | auto-detected | toolchain prefix without the trailing `-`, e.g. `riscv64-unknown-elf` |
| `CFILES` | `main.c` | C sources, basenames only |
| `AFILES` | — | assembly sources, basenames only |
| `CXXFILES` | — | C++ sources, basenames only |
| `BUILD_DIR` | `bin` | object output directory |
| `OPT` | `-Os` | optimisation level |
| `CSTD` | `-std=c99` | C standard |
| `INCLUDES` | — | extra `-I` paths |
| `DEFS` | — | extra `-D` flags |
| `CFLAGS` | — | extra compiler flags |
| `LDFLAGS` | — | extra linker flags |
| `LDLIBS` | — | extra libraries |
| `PROGRAMMER` | `minichlink` | which flasher to drive: `minichlink` or `wchlink` |
| `MINICHLINK` | `minichlink` | minichlink binary |
| `WRITE_SECTION` | `flash` | minichlink write region |
| `WCHLINK` | `wchlink` | the `wchlink` binary, used with `PROGRAMMER=wchlink` |
| `LIBOPENWCH_NOSTDLIB` | — | set to `1` to link with `-nostdlib` instead of newlib |
| `LIBOPENWCH_BLE` | `0` | set to `1` to link WCH's Bluetooth stack, for the `ble_*` layer (CH58x only) |

A new example is a directory with a `main.c` and a `Makefile` no bigger than
this, dropped into `examples/`:

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
`riscv32-unknown-elf`, `riscv-none-elf`, `riscv64-elf` and `riscv32-elf`, in
that order.  Override with:

```sh
make PREFIX=/opt/xpack-riscv-none-elf-gcc/bin/riscv-none-elf
```

`riscv64-linux-gnu-` is not probed: its crt and libc conventions break
bare-metal builds.

## Flashing

[minichlink](https://github.com/cnlohr/ch32fun) drives the WCH-Link and the
built-in USB ISP bootloader, needs no vendor driver, and works on Linux,
Windows and macOS.

```sh
make flash                      # write the internal flash image
make monitor                    # printf over the single-wire debug channel
make unbrick                    # recover a part that stopped answering
```

Point `MINICHLINK` at the binary if it is not on `PATH`:

```sh
make flash MINICHLINK=~/src/ch32fun/minichlink/minichlink
```

### Choosing the programmer

`PROGRAMMER` picks which tool drives the WCH-LinkE:

| Value | Tool |
|---|---|
| `minichlink` | **(default)** minichlink, found on `PATH` or named by `MINICHLINK` |
| `wchlink` | the [libopenwch-tools](https://github.com/LrkSeraph/libopenwch-tools) companion, found on `PATH` |

```sh
make flash PROGRAMMER=wchlink
```

With `PROGRAMMER=wchlink`, `wchlink` is taken from `PATH` unless `WCHLINK`
names a binary; if there is none, the build stops with an explanation rather
than a confusing "command not found".  These examples do not carry the
companion tool — [libopenwch-template](https://github.com/LrkSeraph/libopenwch-template)
is the repository that wires it in as a submodule.

The default stays `minichlink` for now because `wchlink` is still at milestone
1 and cannot flash yet.  It becomes the default once it can.

## Output files

| File | Use |
|---|---|
| `<project>.elf` | debugging, `objdump`, `gdb` |
| `<project>.bin` | `make flash`, and the vendor flash tools |
| `<project>.hex` | the WCH official flash utility |
| `<project>.map` | link map |
| `<project>.list` | disassembly with source (`make <project>.list`) |
| `generated.<device>.ld` | the linker script produced from `ld/devices.data` |

## Notes

* **`-nostartfiles` is mandatory.**  libopenwch supplies its own `_start` and
  vector table, so the toolchain's crt0 must not be linked in.  The rule is
  already in `rules/rules.mk`.
* **Do not add `-march`/`-mabi` by hand.**  They come from `DEVICE` through
  `ld/devices.data`, so the application is always built for the same ISA as the
  library it links against.  Mixing `ilp32` and `ilp32e` objects silently
  corrupts the ABI.
* **`gpio_set_mode()` takes one opaque nibble**, not libopencm3's
  `(mode, cnf)` pair.  See
  `include/libopenwch/ch32v0/common/gpio_common_v1.h` for why.
* **`LIBOPENWCH_BLE=1` links WCH's closed-source Bluetooth stack**, which is
  not part of `libopenwch_ch5xx58x.a` and is Apache-2.0 rather than LGPL.  It
  costs roughly 145 KB of flash and needs a heap the application declares
  with `BLE_HEAP_DEFINE`.  The `ch582_ble_advertise` example sets both
  switches itself, so it builds with a bare `make`.  See `lib/ble/README`.
