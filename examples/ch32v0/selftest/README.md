# CH32V003 self-tests

These programs exercise one peripheral in isolation and print progress and
PASS/FAIL results on USART1 (PD5, 115200 8N1).

| Test | What it checks |
|---|---|
| `dma_selftest` | DMA1 MEM2MEM 8-bit and 32-bit SRAM transfers, plus measured throughput |
| `timer_selftest` | TIM2 1 Hz update period, measured with the SysTick reference counter |

Build the group:

```sh
make
```

Or one test at a time:

```sh
make -C dma_selftest
make -C timer_selftest
```
