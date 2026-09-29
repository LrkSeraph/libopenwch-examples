# CH32V003 integrated self-test

One firmware image exercises on-chip modules and prints PASS/FAIL on USART1
(PD5, 115200 8N1).

| Case | Checks |
|---|---|
| `rcc clock tree` | 48 MHz HSI-PLL, prescalers, published frequencies, `rcc_measure_clocks()` |
| `systick reference` | 48 MHz source, 1 ms tick, counter gate |
| `gpio output/input` | CFGLR nibbles, OUTDR, BSHR/BCR, toggle/write |
| `usart1` | baud divider, frame bits, parity, stop, clock/flow, TC |
| `dma1 mem2mem` | 8/16/32-bit SRAM transfers, offsets, flags, fields |
| `tim1/tim2` | 1 Hz periods, PSC/ATRLR/CNT, OC/IC fields |
| `adc1 internal` | calibration, regular/injected, offset, IRQ/DMA |
| `spi1` | master fields, software NSS, CRC, DMA/IRQ, TXE |
| `i2c1` | FREQ/CCR, own address, PE/ACK/PEC/DMA/IRQ |
| `esig/exten` | flash size, 96-bit UID, lockup reset, LDO mode |
| `IWDG` / `WWDG` | deliberate resets, state kept in `.noinit` |

IWDG/WWDG intentionally reset the part; earlier results are retained in
`.noinit` until the final summary.

```sh
make
make flash          # PROGRAMMER=wchlink or minichlink
```

`DEVICE` can be another CH32V00x sibling, but the image is sized for
CH32V003F4P6 (16 KiB flash / 2 KiB RAM).
