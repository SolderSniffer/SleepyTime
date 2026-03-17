# SleepyTime

An ultra-low-power smartwatch built around the Seeed XIAO nRF54L15 module and a 1.54" e-paper display, designed for maximum standby time and BLE connectivity.

## Overview

SleepyTime is firmware that prioritizes battery life through intelligent power management and the use of low-power display technology. By leveraging the nRF54L15's advanced power modes and a bistable e-paper display, the watch can achieve standby times measured in months rather than days.

## Requirements
- [Docker Engine](https://docs.docker.com/engine/install/) (Linux) or Docker Desktop (macOS/Windows)
- Make (or GNU Make on Windows)

## Getting Started

### 1. Clone Repo and Build the Docker image
```bash
make image
```
This takes ~15 minutes on the first run. Subsequent builds use the layer cache and are much faster.

### 2. Build the firmware
```bash
make build-debug      # debug build with symbols
make build-release    # optimised release build
```

### 3. Flash to hardware
Plug in the XIAO nRF54L15 via USB, then:
```bash
make flash
```

### 4. Debug
The app configures the nRF54L15 to print uart logs to the USB CDC ACM interface. Really handy that Seeed wired one of the uart to the onboard CMSIS-DAP, so no extra USB-serial adapter is needed.

Simply use your serial terminal of choice eg. `minicom`, `putty`, `screen`, `picocom` etc. to connect to the appropriate serial port at 115200 baud.

Why not RTT? From my testing, RTT does not allow the nRF54L15 to properly enter System Off mode. Atleast, it can't wake up from System Off when RTT is active. 

Can I use the CMSIS-DAP for debugging? Yes, and the project is configured to support it. However, I personally just rely on `printf` logs and flashing for development. Also, as explained earlier, an active OpenOCD session may interfere with the device's ability to enter low power modes, which is a critical aspect of this project.

The toys are there, so you can use them if you want! Just be aware of the potential impact on power management.

### 5. Run lint
```bash
make lint             # format-check + clang-tidy (CI gate)
make format           # auto-format sources in-place
```

Formatting is standardised across CLI and editor:
- `make format` / `make lint` use project `.clang-format`

### 6. Run unit tests
```bash
make test             # run unit tests with Unity
```