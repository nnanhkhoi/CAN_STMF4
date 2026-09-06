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
