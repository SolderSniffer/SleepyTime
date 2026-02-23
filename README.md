# SleepyTime

An ultra-low-power smartwatch built around the Nordic nRF54L15 SoC and a 1.54" e-paper display, designed for maximum standby time and BLE connectivity.

## Overview

SleepyTime is a wearable device that prioritizes battery life through intelligent power management and the use of low-power display technology. By leveraging the nRF54L15's advanced power modes and a bistable e-paper display, the watch can achieve standby times measured in months rather than days.

## Requirements
- [Docker Engine](https://docs.docker.com/engine/install/) (Linux) or Docker Desktop (macOS/Windows)
- [VS Code](https://code.visualstudio.com/) with the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension
- For flashing and debugging: a Seeed XIAO nRF54L15 connected via USB

## Getting Started

### 1. Build the dev container image
```bash
docker build -t sleepytime-devenv:local .
```
This takes ~15 minutes on the first run. Subsequent builds use the layer cache and are much faster.

### 2. Open in VS Code
Open the repo folder in VS Code, then when prompted click **Reopen in Container**, or run:

`Ctrl+Shift+P` → **Dev Containers: Reopen in Container**

VS Code will start the container, install extensions, and run a toolchain smoke test. When the terminal shows `--- All tools OK. Run: make help ---` you're ready.

### 3. Build the firmware
```bash
make build-debug      # debug build with symbols
make build-release    # optimised release build
```

### 4. Flash to hardware
Plug in the XIAO nRF54L15 via USB, then:
```bash
make flash
```

### 5. Debug
Press `F5` in VS Code to start a debug session. The firmware is flashed and execution halts at `main`. You can set breakpoints, step through code, and inspect variables as normal. The debug configuration uses OpenOCD with hardware breakpoints to work around the nRF54L15's SPU restrictions.

### 6. View RTT logs
Flash the firmware with `make flash`, then in VS Code open the **Run and Debug** sidebar (`Ctrl+Shift+D`) and click the **Attach and RTT (OpenOCD)** launch configuration. This will connect to the device's RTT output and display log messages in a new terminal panel. In the hovering menu, click the **Reset device** button to reset the device and start streaming logs from the beginning of `main()`.

### 7. Run lint
```bash
make lint             # format-check + clang-tidy (CI gate)
make format           # auto-format sources in-place
```

Formatting is standardised across CLI and editor:
- `make format` / `make lint` use project `.clang-format`
- VS Code right-click **Format Document** also uses `.clang-format` via workspace settings in `.vscode/settings.json`