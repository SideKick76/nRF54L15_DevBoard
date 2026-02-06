# Nordic nRF54 Project Context

## Project Structure
```
/nrf54-projects/
├── Dockerfile
├── docker-compose.yml
├── projects/
│   └── [project-name]/
│       ├── REQUIREMENTS.md   # Project-specific requirements
│       ├── CMakeLists.txt
│       ├── prj.conf
│       └── src/main.c
```

Each project is self-contained with its own REQUIREMENTS.md.

## Development Workflow
1. Create new project folder in `projects/`
2. Add REQUIREMENTS.md with project specs
3. Generate code in the project folder
4. Build: `docker exec nrf54-dev west build -b nrf54l15dk/nrf54l15/cpuapp /workspace/projects/[project-name]`
5. Flash: `docker exec nrf54-dev west flash`

## Creating a New Project
```bash
# Copy existing project as template
cp -r projects/ble-hello projects/new-project
# Edit REQUIREMENTS.md, prj.conf, and CMakeLists.txt for new project
```

## Conventions
- Use Zephyr coding style (kernel style)
- Each project has its own REQUIREMENTS.md
- Update project's REQUIREMENTS.md when adding features
- Test before marking complete

## Docker Commands
```bash
# Build Docker image (one-time, takes ~10 min)
docker build -t nrf54-dev .

# Start container (after attaching USB)
docker-compose up -d

# Build project
docker exec nrf54-dev west build -b nrf54l15dk/nrf54l15/cpuapp /workspace/projects/[name]

# Flash firmware
docker exec nrf54-dev west flash

# Clean build
docker exec nrf54-dev west build -t pristine

# Monitor serial (from WSL2, not Docker)
screen /dev/ttyACM0 115200

# Enter container shell
docker exec -it nrf54-dev bash

# List connected devices
docker exec nrf54-dev nrfutil device list
```

## USB Setup (Windows)
Before starting container, attach J-Link to WSL2:
```powershell
# PowerShell (Admin)
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

## Important
- Check the project's REQUIREMENTS.md before starting
- Match GPIO pins to nRF54L15-DK pinout
- Default UART baud: 115200
- nRF54L15 uses Cortex-M33 with TrustZone
- Attach USB before starting Docker container
