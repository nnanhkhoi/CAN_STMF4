# CAN Normal Mode F4

STM32CubeIDE project implementing a **UDS (Unified Diagnostic Services) server** over CAN bus on an **STM32F407** MCU. The firmware receives UDS requests via CAN (normal mode, 500 kbps) and dispatches them to service handlers organized by ISO 14229 functional units.

## CANable and Cangaroo setup

1. Use the **SLCAN** driver for the CANable.
2. Connect the jumper on the CANable to enable its integrated **120 ohm termination resistor**.
3. Configure Cangaroo to use the CANable through **SLCAN**. Cangaroo only works with the CANable when it is using the SLCAN driver.

The CAN bus should be terminated with 120 ohms at each physical end of the bus. Enable the CANable resistor only when the CANable is located at one of those ends and the other required termination is present at the opposite end.

### Wiring diagram

```text
						CAN bus
				CANH ------------------------------- CANH
			    /                                      |
PC running      CANable                                  | pin 7
Cangaroo          |                                      MCP2551
	|             |                                      | pin 6
	+-- USB ------+-- CANL ------------------------------- CANL
		SLCAN

CANable termination jumper: connect it only when CANable is at one end
of the bus and the other 120 ohm terminator is at the opposite end.

STM32F407                              MCP2551 transceiver
PD1  CAN_TX --------------------------> pin 1 TXD
PD0  CAN_RX <-------------------------- pin 4 RXD
							    pin 7 CANH ---- CANH bus
							    pin 6 CANL ---- CANL bus
							    pin 3 VDD ----- +5 V
							    pin 2 VSS ----- GND
```

The CANable and MCP2551 are two nodes on the same CANH/CANL bus; they are not connected in series. Keep CANH connected to CANH and CANL connected to CANL.


## Architecture

### UDS Service Dispatch

`HAL_CAN_RxFifo0MsgPendingCallback()` (in `main.c`) reads exactly one frame and copies its header and eight data bytes into a 16-frame queue. It never parses requests, runs services, transmits, logs, delays, or waits for the FIFO to empty. When the queue is full, it releases the received hardware frame, drops that newest frame, and records an overflow flag and a saturating drop count. Read failures and hardware FIFO overruns are also recorded; the CAN status/error interrupt is enabled.

The foreground loop removes one frame per iteration and calls `process_can_frame()`. The existing format uses the first byte as the UDS Service ID (SID); remote frames and frames shorter than two bytes are ignored by the dispatcher. Service handlers and `send_can_message()` now run in foreground context, as does UART logging. A short critical section protects each queue removal and each error snapshot, restoring the previous interrupt state before application work. Error and drop reports run at most once per second.

The queue absorbs bursts, but sustained input faster than foreground processing can still cause drops, especially during blocking UART output. The ISR has fixed work per entry; worst-case timing on the board still needs measurement.

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
