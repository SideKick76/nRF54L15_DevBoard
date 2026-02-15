# nRF54L15 DevBoard Projects

Firmware projects for the **nRF54L15 Development Kit** (PCA10156), built with Zephyr RTOS and nRF Connect SDK in a Dockerized environment.

## Current Projects

### BLE HID Media Remote (`projects/ble-hello`)
NFC-triggered BLE HID Consumer Control device. Tap your phone on the NFC antenna to start BLE advertising, pair, and use the 4 buttons as media controls:

| Button | Function |
|--------|----------|
| Button1 | Volume Up |
| Button2 | Volume Down |
| Button3 | Play/Pause |
| Button4 | Next Track |

Hold Button4 at boot to clear all bonds.

## Quick Start (New PC)

### Prerequisites
- Windows with WSL2
- Docker Desktop (with WSL2 backend enabled)

### One-liner install
Open WSL and run:
```bash
curl -fsSL https://raw.githubusercontent.com/SideKick76/nRF54L15_DevBoard/main/setup-new-pc.sh | bash
```

This installs all dependencies, clones the repo, and builds the Docker image.

### Manual setup
```bash
git clone git@github.com:SideKick76/nRF54L15_DevBoard.git
cd nRF54L15_DevBoard
docker build -t nrf54-dev .
docker-compose up -d
```

## USB Setup (Windows)

Before building/flashing, attach the J-Link debugger to WSL2:

```powershell
# PowerShell (Admin)
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

## Build & Flash

```bash
# Build a project
docker exec nrf54-dev west build -b nrf54l15dk/nrf54l15/cpuapp /workspace/projects/ble-hello

# Flash to device
docker exec nrf54-dev west flash

# Clean build
docker exec nrf54-dev west build -t pristine
```

## Development with Claude Code

This repo includes project context (`.claude/CLAUDE.md`) for use with [Claude Code](https://claude.com/claude-code). To start:

```bash
cd ~/nRF54L15_DevBoard
claude
```

Claude Code will automatically pick up the project context and conventions.

## Adding a New Project

```bash
cp -r projects/ble-hello projects/new-project
# Edit REQUIREMENTS.md, prj.conf, CMakeLists.txt, and src/main.c
```

Each project is self-contained with its own `REQUIREMENTS.md`.
