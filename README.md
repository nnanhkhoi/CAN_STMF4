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

### FreeRTOS periodic runnable groups

The firmware uses the FreeRTOS V11.3.1 kernel vendored under `Middlewares/Third_Party/FreeRTOS`. The GCC Cortex-M4F
port uses the existing hard-float toolchain. This is an AUTOSAR-style
task/runnable organization, not an AUTOSAR OS, RTE, or conformance implementation.

| Task | Period | FreeRTOS priority | Stack | Runnable group |
|---|---|---|---|---|
| `Task_10ms` | 10 ms | 4 (highest application priority) | 4096 bytes | CAN receive / CanTp / UDS dispatch |
| `Task_20ms` | 20 ms | 3 | 1024 bytes | Application extension point |
| `Task_50ms` | 50 ms | 2 | 2048 bytes | CAN error reporting, limited to once per second |
| `UART_Log` | Queue driven | 1 | 1024 bytes | Serial output |
| FreeRTOS idle | When other tasks block | 0 | 512 bytes | Kernel idle task |

Add function calls to `App_Runnables10ms()`, `App_Runnables20ms()`, or
`App_Runnables50ms()` in [app_runnables.c](Core/BSW/Os/app_runnables.c). Calls in
one group execute sequentially in source order. Higher-priority groups can
preempt lower-priority groups. Keep runnables short and nonblocking; use queues
or protected snapshots for data shared between groups. UDS session state,
service handlers, CAN response transmission, and the existing diagnostic
`malloc`/`free` calls belong exclusively to the 10 ms group.

Periods, priorities, and static stack allocations are in
[rtos.c](Core/BSW/Os/rtos.c). `vTaskDelayUntil()` uses a common epoch:
first activations occur at 10, 20, and 50 ms after scheduler startup. When due
together, the 10 ms task runs first. Execution time does not accumulate into
the normal release interval. A group that finishes at or after its next deadline
records a miss and waits one full period from completion, avoiding catch-up bursts.

`App_RtosGetStats()` returns activation count, deadline misses, and maximum
elapsed execution time (including preemption, in 1 ms ticks). Sub-millisecond
execution can read as zero. `App_RtosGetStackHeadroom()` returns minimum unused
stack in 32-bit words. Call these APIs from task context, or inspect
`periodic_tasks_st` in the debugger. Assertions and stack overflow checks stop in
`Error_Handler()`; inspect `app_rtos_assert_file`, `app_rtos_assert_line`, and
`app_rtos_overflow_task` to diagnose failures.

Tasks and the log queue use static allocation; no FreeRTOS heap is needed.
The task stacks occupy 8704 bytes of CCM RAM in the `.rtos_stacks` NOLOAD
section, preserving main SRAM for the existing diagnostic tables and C heap.
FreeRTOS initializes each stack before use. CCM is CPU-only: stack buffers
must not be passed to DMA; use main-SRAM buffers if adding DMA later.

`SysTick_Handler()` advances the HAL tick at 1 kHz before and after scheduler
startup, and also advances FreeRTOS once started. SVC and PendSV are supplied
directly by the kernel port. Keep the tick at 1 kHz and tickless idle disabled;
do not suspend the HAL tick while the scheduler is running. CAN interrupts
remain at NVIC priority 0 and call no RTOS APIs. If adding `...FromISR()` calls,
first change those IRQs to NVIC priorities 5 through 15. Preserve the existing
PRIMASK critical sections around the CAN ring buffer: a normal FreeRTOS
critical section does not mask priority-0 CAN interrupts.

After scheduler startup, `UART_Send()` copies up to 223 characters to a
16-message queue without waiting. Only `UART_Log` transmits, with a 50 ms HAL
timeout. Full queues drop new log messages; inspect `App_LogGetDroppedCount()`
and `App_LogGetErrorCount()`. Logging can lose messages under sustained load,
but serial output does not block the periodic groups. Do not call `UART_Send()`
from interrupt context. CanTp is initialized before scheduling. The ECU no longer sends a tester request at startup.

### CAN receive and UDS dispatch

`HAL_CAN_RxFifo0MsgPendingCallback()` in `Core/BSW/Can/app_can.c` reads one
hardware FIFO frame per callback. Only standard data frames addressed to
`CANTP_RX_ID` enter its 16-frame software queue. The ISR never calls CanTp,
UDS, FreeRTOS, UART, or a blocking transmit operation.

`App_CanMainFunction10ms()` removes at most four frames and passes them to
`CanTp_RxIndication()`, then calls `CanTp_MainFunction(HAL_GetTick())` once.
CanTp strips PCI bytes and calls `CanTp_RxComplete()` in
`Core/Diagnostic/uds_transport.c` only after the entire payload is available.
The dispatcher validates lengths and decodes structured requests into aligned
local objects. Existing handlers still run exclusively in the 10 ms task.

Responses go through `send_can_message(const uint8_t *, uint16_t)`, which copies
one complete UDS payload into CanTp. It must not be called for individual CAN
fragments. The DTC list encoder now builds one response instead of sending
several independent fragments. A second response while one is active is rejected
and counted; there is no response queue. Reset/power actions scheduled by the
ECUReset handler execute only after successful final TX confirmation, and are
discarded if transmission fails.

`CommunicationControl` (`0x28`) currently returns NRC `0x22` for its recognized
subfunctions until an application normal-communication hook is implemented.
It must not disable diagnostic RX interrupts or enable TX-empty interrupts,
which would interrupt ISO-TP reception or consume mailbox confirmations.

Queue overflow, FIFO overrun, CAN read failure, or bus-off sets an ISR flag.
The 10 ms task discards the affected queue and cancels transport state before
accepting a fresh transfer. No incomplete request is dispatched. CAN hardware
bus-off recovery remains an application responsibility (`AutoBusOff` is disabled).
The 50 ms group reports CAN errors and dropped frames at most once per second.

### CanTp / ISO-TP configuration

This is a compact project-specific ISO-TP transport for classic CAN, organized
as BSW. It is not a complete AUTOSAR CanTp/PduR/DCM implementation or a certified
ISO 15765-2 conformance package. The framing and flow-control behavior follow
[Linux ISO-TP documentation](https://kernel.org/doc/html/latest/networking/iso15765-2.html);
timer terminology is described in the
[AUTOSAR CAN Transport Layer specification](https://www.autosar.org/fileadmin/standards/R24-11/CP/AUTOSAR_CP_SWS_CANTransportLayer.pdf).

```text
CAN RX interrupt -> bounded frame queue -> 10 ms task -> CanTp -> UDS handler
                                                          ^           |
                                                          |    complete response
                                                          +-----------+
                                                          |
                                                   bxCAN mailbox
```

| File | Responsibility |
|---|---|
| `Core/BSW/CanTp/CanTp_Cfg.h` | CAN IDs, buffer capacity, flow control and timeout values |
| `Core/BSW/CanTp/CanTp.h/.c` | Portable transport state machine, static RX/TX buffers and counters |
| `Core/BSW/CanTp/CanTp_Port.h/.c` | Nonblocking STM32 HAL mailbox submission, confirmation polling and abort |
| `Core/Diagnostic/uds_transport.h/.c` | Complete-payload dispatch, response submission and deferred ECU actions |

| Setting | Default |
|---|---|
| Physical request / tester TX ID | `0x7E0` |
| Physical response / ECU TX ID | `0x7E8` (also used for ECU flow control) |
| Addressing | Normal addressing, 11-bit CAN ID, one half-duplex channel |
| Payload capacity | 1024 bytes per direction; compile-time range 8..4095 |
| Receiver block size | 4 consecutive frames per CTS |
| Receiver STmin | 10 ms (`0x0A`) |
| N_As / N_Ar | 1000 ms total for a scheduled data / flow-control frame, including mailbox retries |
| N_Bs / N_Cr | 1000 ms waiting for FC / next CF |
| Maximum consecutive FC WAIT | 3; the fourth aborts TX |
| TX DLC / padding | 8 bytes / `0x00` |
| Execution context | Existing 10 ms task; no additional task and no dynamic allocation |

The old raw-UDS-on-CAN format is replaced by ISO-TP: the first CAN data byte is
now PCI, not SID. `0x7DF` functional addressing, extended/mixed addressing, 29-bit
IDs, CAN FD and the 32-bit FF length extension are outside this implementation.
RX accepts unpadded SF/final CF when all declared payload bytes are present;
FF and nonfinal CF require eight bytes. Extra RX padding is ignored.

SF carries 1..7 payload bytes. FF starts an 8..4095-byte transfer; a payload
larger than the configured buffer gets FC Overflow (`32 00 00 ...`). CF sequence
numbers advance modulo 16. The receiver sends CTS at the beginning and each
block boundary. Wrong sequence, malformed length, missing frames or timeout
abort the partial transfer. A new SF/FF replaces a partial RX transfer. While
transmitting a response, new requests are ignored; FC remains accepted.

The sender honors peer BS (including zero for unlimited), CTS, WAIT, Overflow
and STmin. STmin `00..7F` represents milliseconds; `F1..F9` is rounded up to
1 ms; reserved values use 127 ms. Actual sends occur on 10 ms activations, so
spacing can be longer than requested. There is no busy-wait for precise sub-ms
spacing. Timing starts from the task's observation of hardware completion,
which is conservative; measure actual throughput and deadlines on the board.
Only one mailbox is outstanding, and a busy mailbox is retried on later cycles.
A missing CAN ACK is bounded by the confirmation timeout and abort request.

TX completion is polled using the owned mailbox's `TXOK` bit; a mailbox becoming
empty alone does not prove success. Keep `CAN_IT_TX_MAILBOX_EMPTY` disabled so
HAL interrupt processing cannot clear the completion flags first. RX and error
interrupts remain enabled. All CanTp APIs and upper callbacks run in the 10 ms
task, except the initial `CanTp_Init()` before scheduler startup. Read
`CanTp_GetStats()` there or inspect the counters in the debugger: successful
RX/TX, RX/TX errors, timeouts and rejected TX submissions.

The transport accepts up to 1024 bytes, but application handlers keep their own
limits: legacy byte-length handlers accept at most 256 bytes including SID;
TransferData/TransferExit records are limited to their 255/256-byte arrays;
file paths are at most 255 bytes; RoutineControl currently models one option
byte. Oversized application requests receive NRC `0x13`, without length
truncation. Existing service/security/storage/file-transfer logic contains demo
stubs and is not made production-complete by adding transport. No automatic UDS
`0x78` response-pending or retransmission of an aborted payload is provided.

### CanTp examples with Cangaroo / a raw CAN tester

Use the existing 500 kbps bus. The examples below show eight data bytes per
frame; SID is part of the ISO-TP payload. The tester must send FC automatically
or within 1000 ms when the ECU sends FF. For tester multi-frame requests, wait
for ECU CTS, obey its BS and STmin, and do not interleave another request.

**Single-frame DiagnosticSessionControl:**

```text
Tester -> ECU  7E0  02 10 03 00 00 00 00 00
ECU -> Tester  7E8  06 50 03 00 32 01 F4 00
```

**Single-frame request with multi-frame response (demo DID F187):**

```text
Tester -> ECU  7E0  03 22 F1 87 00 00 00 00
ECU -> Tester  7E8  10 0C 62 F1 87 DE AD AD   # FF: 12 payload bytes
Tester -> ECU  7E0  30 00 0A 00 00 00 00 00   # CTS: unlimited block, 10 ms STmin
ECU -> Tester  7E8  21 AD AD AD AD AD AD 00
```

**Multi-frame request and response (four DID records):**

```text
Tester -> ECU  7E0  10 09 22 10 01 10 02 10   # FF: 9-byte request
ECU -> Tester  7E8  30 04 0A 00 00 00 00 00   # CTS: BS=4, STmin=10 ms
Tester -> ECU  7E0  21 03 10 01 00 00 00 00
ECU -> Tester  7E8  10 11 62 10 01 12 34 10   # FF: 17-byte response
Tester -> ECU  7E0  30 00 0A 00 00 00 00 00
ECU -> Tester  7E8  21 02 56 78 10 03 9A BC
ECU -> Tester  7E8  22 10 01 12 34 00 00 00
```

For a new service, pass the full response without PCI bytes:

```c
/* Call from the owning 10 ms task; CanTp copies the buffer before returning. */
const uint8_t response[] = {0x62, 0x10, 0x01, 0x12, 0x34};
CanTp_Result_t result = CanTp_Transmit(response, sizeof(response), HAL_GetTick());
/* Handle CANTP_BUSY / CANTP_INVALID in the application; CANTP_OK is acceptance.
 * CanTp_TxComplete(success) reports the later on-bus outcome. */
```

### Functional Units (Core/Diagnostic/)

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

## Build and validation

With GNU Make and `arm-none-eabi-*` on PATH (GCC 11 or newer for the linker script):

```sh
make -j4
```

STM32CubeIDE Debug and Release include the middleware source directory and both
FreeRTOS include paths in `.cproject`. Clean/rebuild after refreshing the project.
This is a manually maintained native FreeRTOS integration: the `.ioc` does not
enable the CubeMX CMSIS-RTOS generator. It disables generated SVC/PendSV bodies
because the kernel provides them. After CubeMX regeneration, check `.cproject`,
both linker scripts' `.rtos_stacks` section, and the SysTick user block before
rebuilding; do not add a second generated RTOS startup or tick handler.

Run the native CanTp tests with MinGW GCC on Windows (no board required):

```powershell
powershell -ExecutionPolicy Bypass -File tests/run_cantp_tests.ps1
```

The protocol tests cover SF validation, copied TX buffer lifetime, FF/CF
segmentation and reassembly up to 1024 bytes, sequence wrap, BS=0/1/4, STmin,
WAIT limits, overflow, malformed frames, mailbox failure, cancellation, all four
timeouts and tick rollover. A separate port test checks all three mailbox result
bits, asynchronous abort and bus-off rejection. The fake port does not replace
on-board CAN timing/ACK/error testing. Tests are outside the CubeIDE source roots.

The integration suite links the real UDS modules with fake HAL/time/UART services.
It checks the README frame traces, application length rejection, repeated-DID
response bounds, CommunicationControl rejection, and reset only after successful
TX confirmation (never after a timeout).

On hardware, inspect activation counts over 10 seconds (approximately 1000,
500, and 200); verify zero deadline misses and adequate stack headroom under
CAN traffic. Test CAN bursts, receive overflow, a disconnected bus, and log
queue saturation. Measure GPIO pulses around runnables if sub-millisecond
jitter/WCET is required. A successful build and image check do not establish
real-time timing or validate the existing UDS service implementations.
