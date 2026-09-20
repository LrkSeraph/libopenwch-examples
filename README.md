# libopenwch application template

A starting point for a firmware project built on
[libopenwch](https://github.com/LrkSeraph/libopenwch).  Clone it as your
project, point it at a libopenwch checkout, and start writing `main.c`.

This is the RISC-V/WCH counterpart of
[libopencm3-template](https://github.com/bonedaddy/libopencm3-template): the
same `rules.mk` idea, the same `PROJECT`/`DEVICE`/`OPENWCH_DIR` variables, and
a `make flash` that works without vendor tooling.

It is a **separate repository** from the library, for the same reason
libopencm3-template is separate from libopencm3: the library is something you
build *against*, this is something you build *from*.  No driver code lives
here.

## Quick start

```sh
# 1. Get libopenwch (next to where your project will live)
git clone https://github.com/LrkSeraph/libopenwch.git ~/src/libopenwch

# 2. Clone this template as your project
git clone https://github.com/LrkSeraph/libopenwch-template.git ~/src/my-firmware
cd ~/src/my-firmware

# 3. Build an example
cd examples/blink
make                                # finds the sibling libopenwch checkout
make OPENWCH_DIR=~/src/libopenwch   # or say where it is explicitly

# 4. Flash it (needs a WCH-Link programmer, see below)
make flash
```

`OPENWCH_DIR` is only guessed when you have not set it.  The guess tries
`../libopenwch` next to this template and then the parent directory, confirming
each by looking for `mk/genlink-config.mk`; if neither matches, the build stops
and names the paths it tried.  Setting `OPENWCH_DIR` — on the command line, in
the environment, or in your own `Makefile` — always wins.

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

This repository is meant to become your project: keep the checkout, rename it
if you like, and delete the examples you do not need.  If you would rather keep
them as references, copy one instead:

```sh
cp -r examples/blink examples/my_app
$EDITOR examples/my_app/main.c
cd examples/my_app && make flash
```

`main.c` only has to provide `int main(void)`.  Everything else — the reset
path, `.data`/`.bss` initialisation, the vector table and the entry point —
comes from libopenwch's QingKe core layer.

## Layout

```
libopenwch-template/
├── README.md  NOTICE  LICENSE
├── .clang-format            house style, shared with libopenwch
├── .gitignore
├── .vscode/                 editor configuration (IntelliSense, debug, tasks)
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

There is deliberately no top-level `Makefile`: a firmware project is one
directory with one `Makefile`, and each example is independent.  A top-level
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
| `OPENWCH_DIR` | sibling of the template | path to the libopenwch checkout |
| `TEMPLATE_DIR` | `../..` from the example | path to this template directory |
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
| `WCHLINK` | `tools/wchlink/build/wchlink` | the `wchlink` binary, used with `PROGRAMMER=wchlink` |
| `LIBOPENWCH_NOSTDLIB` | — | set to `1` to link with `-nostdlib` instead of newlib |
| `LIBOPENWCH_BLE` | `0` | set to `1` to link WCH's Bluetooth stack, for the `ble_*` layer (CH58x only) |

A minimal `Makefile` for a new project is therefore just:

```make
PROJECT     = my_app
DEVICE      = ch32v003f4p6
TEMPLATE_DIR ?= $(abspath ../..)
OPENWCH_DIR  ?= $(abspath $(TEMPLATE_DIR)/..)
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
| `wchlink` | the libopenwch-tools companion, from the `tools/wchlink/` submodule or on `PATH` |

```sh
make flash PROGRAMMER=wchlink
```

With `PROGRAMMER=wchlink`, the built submodule binary
(`tools/wchlink/build/wchlink`) is used if it exists, otherwise one on `PATH`;
if there is neither, the build stops with an explanation rather than a
confusing "command not found".

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
