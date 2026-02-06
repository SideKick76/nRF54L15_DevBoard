# Nordic nRF54 Project Context

## Project Structure
```
/nrf54-projects/
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
4. Build: `west build -b nrf54l15dk/nrf54l15/cpuapp projects/[project-name]`
5. Flash: `west flash`

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

## Common Commands
```bash
# Build project
west build -b nrf54l15dk/nrf54l15/cpuapp projects/[name]

# Flash firmware
west flash

# Clean build
west build -t pristine

# Monitor serial (115200 baud)
# Use screen, minicom, or nRF Connect Serial Terminal
screen /dev/ttyACM0 115200
```

## nRF Connect SDK Setup
```bash
# Install west
pip install west

# Initialize SDK (one-time)
west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.7.0 ncs
cd ncs
west update

# Set environment
source ncs/zephyr/zephyr-env.sh
```

## Important
- Check the project's REQUIREMENTS.md before starting
- Match GPIO pins to nRF54L15-DK pinout
- Default UART baud: 115200
- nRF54L15 uses Cortex-M33 with TrustZone
