# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

STM32CubeIDE project implementing a **UDS (Unified Diagnostic Services) server** over CAN bus on an **STM32F407VGT6** microcontroller. The firmware receives UDS requests via CAN1 (normal mode, 125 kbps) and dispatches them to service handlers organized by ISO 14229 functional units.

## Build

This is an STM32CubeIDE project using GNU Tools for STM32 (arm-none-eabi-gcc 13.3). Build from the `Debug/` directory:

```bash
cd Debug && make all      # Full build
cd Debug && make clean    # Clean build artifacts
```

The build produces `Debug/CAN_NormalModeF4.elf`. Toolchain: MCU ARM GCC with `-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard`. Linker script: `STM32F407VGTX_FLASH.ld`.

## Hardware Configuration (from .ioc)

- **MCU**: STM32F407VGTx (LQFP100), system clock 24 MHz (HSI + PLL)
- **CAN1**: Pins PD0 (RX) / PD1 (TX), 125 kbps, normal mode, auto bus-off recovery, auto wake-up, no auto retransmission disabled (NART=ENABLE in .ioc but code sets AutoRetransmission=ENABLE)
- **USART3**: Pins PB10 (TX) / PB11 (RX), 115200 baud — used for debug logging via `UART_Send()`
- **TIM6**: Basic timer (not actively used in main loop currently)
- CAN filter accepts all IDs (mask = 0x0000). CAN RX uses FIFO0 with interrupt-driven reception.

## Architecture

### UDS Service Dispatch

All CAN messages arrive in `HAL_CAN_RxFifo0MsgPendingCallback()` (in `main.c`). The first byte (`rcvd_msg[0]`) is the UDS Service ID (SID), which dispatches to the appropriate handler. Responses are sent via `send_can_message()` (declared in `Data_Transmission_functional_unit.h`).

### Functional Units (Core/Src/ and Core/Inc/)

The UDS implementation is split into functional units per ISO 14229:

| Functional Unit | SIDs | Files |
|---|---|---|
| **Diagnostic Communication Management** | 0x10, 0x11, 0x27, 0x28, 0x3E, 0x83, 0x84, 0x85, 0x86, 0x87 | `Diagnostic_Communication_Management_functional_unit.c/.h` |
| **Data Transmission** | 0x22, 0x2A, 0x2C, 0x2E | `Data_Transmission_functional_unit.c/.h` |
| **Stored Data Transmission** | 0x14, 0x19 | `Stored_Data_Transmission_functional_unit.c/.h` |
| **Input/Output Control** | 0x2F | `InputOutput_Control_functional_unit.c/.h` |
| **Routine Control** | 0x31 | `Routine_functional_unit.c/.h` |
| **Upload/Download** | 0x34, 0x35, 0x36, 0x37, 0x38 | `Upload_Download_functional_unit.c/.h` |

Common NRC codes and the umbrella include are in `uds_services.h/.c`.

### Session and Security Model

- `UDS_Session` struct tracks `current_session` and `security_access_granted`
- Sessions: Default (0x01), Programming (0x02), Extended Diagnostic (0x03), Safety System (0x04)
- Security Access uses seed/key at two levels
- `is_service_allowed()` gates services based on current session

### Key Conventions

- Each UDS service handler follows the pattern: validate request, check session/security, send positive or negative response
- Response functions are named `send_positive_response_<service>()` / `send_negative_response_<service>()`
- Comments are primarily in French
- CAN TX uses StdId 0x7DF for test frames, 0x123 for cyclic debug

## STM32CubeMX Code Generation

The `.ioc` file is the CubeMX configuration source. When regenerating code, user code must be placed within `/* USER CODE BEGIN */` / `/* USER CODE END */` blocks to survive regeneration. Peripheral init functions (`MX_*_Init`) and `SystemClock_Config` are auto-generated.
