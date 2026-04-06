# ADS131M08 Linux Driver

A custom Linux kernel driver for the Texas Instruments **ADS131M08** 8-channel, 24-bit ADC.

This driver is implemented as an **SPI-based character device driver** intended for low-latency, interrupt-driven data acquisition. It captures ADC samples on the **DRDY** signal, transfers frames over SPI, parses the 24-bit channel data, stores samples in kernel-side chunk buffers, and exposes the data to user space through **`/dev/ads131m08`**.

---

## Features

- SPI-based driver for **ADS131M08**
- Interrupt-driven sampling using **DRDY GPIO**
- Character device interface via **`/dev/ads131m08`**
- Blocking **`read()`** support for chunked sample delivery
- **`ioctl()`** interface for ADC control and register access
- Internal multi-buffer chunk management for stable streaming
- Optional status exposure through **`/proc/ads131m08`**
- User-space test program for bring-up and validation

---

## Driver Overview

The driver is designed around the following acquisition flow:

1. The ADS131M08 asserts **DRDY** when a new conversion result is ready.
2. A GPIO interrupt handler is triggered on the DRDY edge.
3. The driver reads one SPI frame from the ADC.
4. The frame is parsed into one timestamped sample containing **8 channels**.
5. Samples are accumulated into a kernel chunk buffer.
6. When a chunk is full, it is moved to the ready queue.
7. A blocked user-space reader is woken up and receives the chunk through `read()`.

This design is suitable for continuous acquisition workloads where user space consumes data in fixed-size batches rather than one sample at a time.

---

## Repository Structure

The source tree is organized into small functional units.

```text
ads131m08_main.c      Driver probe/remove and module entry points
ads131m08_spi.c       SPI transfer, command handling, register read/write
ads131m08_gpio.c      GPIO setup, DRDY IRQ handling, reset pin control
ads131m08_data.c      Runtime state, chunk buffers, ready/free/active lists
ads131m08_iface.c     Character device interface, read/poll/ioctl, /proc support
list.c / list.h       Internal linked-list helper
queue.c / queue.h     Internal queue helper (if included in your tree)
test_program/         User-space test utilities
ads131m08*.dts        Device Tree overlay(s) / board-specific DTS snippets
Makefile              External kernel module build file
```

Depending on the exact revision of your source tree, some filenames may differ slightly, but the logical split above matches the driver architecture.

---

## Runtime Architecture

### Kernel-side data path

```text
ADS131M08 DRDY GPIO
        ↓
 IRQ / threaded handler
        ↓
    SPI frame read
        ↓
  24-bit sample parsing
        ↓
 timestamped smpl_data_t
        ↓
  active chunk buffer fill
        ↓
 full chunk → ready list
        ↓
 wake_up_interruptible()
        ↓
 user-space read()
```

### Buffer lifecycle

The driver uses a chunk-oriented buffer model similar to a small ring pipeline:

```text
free list  →  active list  →  ready list  →  user-space read()  →  free list
```

A typical configuration uses multiple chunk buffers so sampling can continue while user space consumes previously completed chunks.

---

## Sample Format

The driver converts each ADC frame into a structure equivalent to the following:

```c
typedef struct {
    ktime_t ts;      /* kernel timestamp */
    int32_t data[8]; /* sign-extended 24-bit ADC data */
} smpl_data_t;
```

### Notes

- `ts` is captured in kernel space when the sample is processed.
- `data[8]` contains one signed value for each ADS131M08 input channel.
- Each 24-bit ADC word is sign-extended to 32-bit integer format before being passed to user space.

---

## Device Node and Interfaces

### Character device

The driver exposes:

```text
/dev/ads131m08
```

Typical user-space operations:

- `open()`
- `read()`
- `poll()`
- `ioctl()`
- `close()`

### Proc entry

The driver may also create:

```text
/proc/ads131m08
```

This is typically used for lightweight runtime diagnostics such as:

- open/run state
- buffer list status
- chunk usage summary

---

## Supported Control Operations

The driver provides an `ioctl()` interface for basic device control and register access.

### Common control commands

- `ADS_IOCTL_CMD_RESET`
- `ADS_IOCTL_CMD_WAKEUP`
- `ADS_IOCTL_CMD_STANDBY`
- `ADS_IOCTL_CMD_LOCK`
- `ADS_IOCTL_CMD_UNLOCK`
- `ADS_IOCTL_CMD_CHUNK_SZ`
- `ADS_IOCTL_CMD_STAT`
- `ADS_IOCTL_CMD_RREG`
- `ADS_IOCTL_CMD_WREG`

### Register access structure

Register read/write operations use a structure equivalent to:

```c
struct ads_ioctl_reg {
    uint8_t  start_addr;
    uint8_t  reg_count;
    uint16_t data[];
};
```

### Intended use

- Use **WAKEUP** to start or resume conversions.
- Use **STANDBY** to pause acquisition.
- Use **RESET** during bring-up or recovery.
- Use **RREG/WREG** to inspect or configure ADS131M08 registers.
- Use **CHUNK_SZ** to control how many samples are returned per `read()`.

The exact ioctl numbers and macro definitions should be taken from the driver header used by your test program or user application.

---

## Build

The driver is built as an **out-of-tree kernel module**.

### Typical Makefile pattern

```make
obj-m += ads131m08.o

ads131m08-objs := ads131m08_main.o ads131m08_iface.o ads131m08_gpio.o \
                  ads131m08_spi.o ads131m08_data.o list.o
```

If your local source tree also uses `queue.o` or additional helper objects, add them to the object list as needed.

### Build command

```bash
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
```

### Clean command

```bash
make -C /lib/modules/$(uname -r)/build M=$(pwd) clean
```

### Result

```text
ads131m08.ko
```

---

## Loading the Module

### Insert the module

```bash
sudo insmod ads131m08.ko
```

### Check kernel messages

```bash
dmesg | tail -n 50
```

### Verify device node

```bash
ls -l /dev/ads131m08
```

### Remove the module

```bash
sudo rmmod ads131m08
```

If the device node is not created automatically, verify that the probe path completed successfully and that your platform provides the expected SPI and GPIO resources.

---

## Device Tree / Platform Integration

The driver expects platform-specific wiring information to be provided through **Device Tree** (or an equivalent board integration layer).

A typical DTS/overlay configuration contains:

- ADS131M08 node on an SPI bus
- `compatible = "ti,ads131m08"`
- `spi-max-frequency`
- interrupt GPIO / `interrupt-parent`
- `interrupts`
- optional reset GPIO

### Example sketch

```dts
&spi5 {
    status = "okay";

    ads131m08@0 {
        compatible = "ti,ads131m08";
        reg = <0>;
        spi-max-frequency = <1000000>; /* example only */
        interrupt-parent = <&gpio>;
        interrupts = <...>;
        /* reset-gpios = <...>; */
    };
};
```

### Important note

The source history indicates that some revisions of the driver configure **SPI mode 1** and may use a runtime SPI clock up to **15 MHz**, while a DTS overlay example may declare a lower `spi-max-frequency` such as **1 MHz**.

Make sure the following are consistent for your board:

- actual hardware signal integrity
- ADS131M08 timing limits
- Linux SPI controller capabilities
- Device Tree `spi-max-frequency`
- driver-side SPI mode/frequency settings

---

## Basic User-Space Workflow

A typical acquisition sequence is:

1. Open `/dev/ads131m08`
2. Optionally set chunk size through `ioctl()`
3. Send `WAKEUP` to start conversions
4. Repeatedly call `read()` to receive filled chunks
5. Send `STANDBY` when stopping acquisition
6. Close the device

### Pseudo code

```c
int fd = open("/dev/ads131m08", O_RDWR);

unsigned long chunk_sz = 40;
ioctl(fd, ADS_IOCTL_CMD_CHUNK_SZ, &chunk_sz);
ioctl(fd, ADS_IOCTL_CMD_WAKEUP, 0);

while (running) {
    smpl_data_t buf[40];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) {
        /* process samples */
    }
}

ioctl(fd, ADS_IOCTL_CMD_STANDBY, 0);
close(fd);
```

The actual user-space definitions for `smpl_data_t` and ioctl macros must match the driver headers used when compiling the application.

---

## Read Semantics

The driver is designed for **chunk-based blocking reads**.

### Expected behavior

- `read()` blocks until a complete chunk is available
- one completed chunk is copied to user space
- the consumed chunk is returned to the free pool
- `poll()` can be used to wait for readable data

### Practical implication

This interface is optimized for streaming applications that prefer predictable batches rather than immediate per-sample delivery.

---

## Test Program

The source tree includes a `test_program/` directory for validation and bring-up.

Typical test functionality may include:

- sending WAKEUP/STANDBY/RESET commands
- reading/writing registers
- changing chunk size
- receiving streamed sample data from `/dev/ads131m08`
- printing ioctl command information

This is the recommended starting point when verifying new hardware.

---

## Bring-Up Checklist

Before debugging the driver itself, verify the following:

1. **SPI bus is enabled** in the kernel and Device Tree.
2. **Chip select** is wired correctly.
3. **DRDY GPIO** is connected and interrupt-capable.
4. **RESET pin** (if used) is connected correctly.
5. ADS131M08 power rails and reference are valid.
6. SPI mode, word timing, and max frequency match the board design.
7. The Device Tree node matches the bus number and chip-select used in hardware.
8. `dmesg` shows successful probe messages.

---

## Troubleshooting

### No `/dev/ads131m08`

Possible causes:

- probe failed
- Device Tree mismatch
- SPI bus disabled
- incompatible `compatible` string
- missing major/minor registration or cdev setup failure

Check:

```bash
dmesg | grep -i ads131m08
```

### `read()` blocks forever

Possible causes:

- DRDY interrupt never arrives
- ADC not started (`WAKEUP` missing)
- IRQ trigger type is wrong
- SPI transfer fails in the interrupt path
- chunk size is misconfigured relative to application expectations

### Data looks corrupted

Possible causes:

- SPI mode mismatch
- clock too fast for the board
- bit alignment/parsing issue
- wrong sign extension handling in user space
- electrical noise / grounding / reference problems

### Intermittent streaming issues

Possible causes:

- buffer starvation under heavy user-space latency
- race conditions during stop/start control
- incomplete error handling around failed SPI transactions

---

## Design Notes and Known Improvement Areas

Based on the source structure, the current design is practical and easy to debug, but the following areas are worth reviewing for long-term maintenance:

- migrate legacy GPIO handling to **`gpiod_*`** APIs if not already done
- validate list locking and IRQ-safe unlock usage carefully
- review concurrency between `read()`, `ioctl()`, and IRQ-driven buffering
- consider additional runtime statistics (overflow, dropped chunk count, SPI error count)
- consider `kfifo` or other standardized buffering primitives if the implementation grows
- consider DMA-capable SPI paths if higher sustained throughput is required
- add suspend/resume power-management hooks if the platform needs them

---

## Intended Use Case

This driver is a good fit for:

- embedded data loggers
- multi-channel analog acquisition systems
- streaming ADC capture from user space
- rapid hardware bring-up on Linux SBC or custom carrier boards

---

## License

- GPL-2.0-or-later

## Maintainer Notes

When updating this README, keep the following items synchronized with the code:

- module name
- device node path
- ioctl definitions
- sample structure layout
- default chunk size
- buffer count
- SPI mode/frequency
- DTS examples
- test program build/run instructions

If the driver evolves to an IIO-based design in the future, this README should be revised accordingly because the current document assumes a **custom character-device architecture**.
