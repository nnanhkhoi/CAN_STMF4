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

The firmware uses the FreeRTOS V10.3.1 kernel bundled with STM32Cube F4
V1.28.3, vendored under `Middlewares/Third_Party/FreeRTOS`. The GCC Cortex-M4F
port uses the existing hard-float toolchain. This is an AUTOSAR-style
task/runnable organization, not an AUTOSAR OS, RTE, or conformance implementation.

| Task | Period | FreeRTOS priority | Stack | Runnable group |
|---|---|---|---|---|
| `Task_10ms` | 10 ms | 4 (highest application priority) | 4096 bytes | CAN receive / UDS dispatch |
| `Task_20ms` | 20 ms | 3 | 1024 bytes | Application extension point |
| `Task_50ms` | 50 ms | 2 | 2048 bytes | CAN error reporting, limited to once per second |
| `UART_Log` | Queue driven | 1 | 1024 bytes | Serial output |
| FreeRTOS idle | When other tasks block | 0 | 512 bytes | Kernel idle task |

Add function calls to `App_Runnables10ms()`, `App_Runnables20ms()`, or
`App_Runnables50ms()` in [app_runnables.c](Core/Src/app_runnables.c). Calls in
one group execute sequentially in source order. Higher-priority groups can
preempt lower-priority groups. Keep runnables short and nonblocking; use queues
or protected snapshots for data shared between groups. UDS session state,
service handlers, CAN response transmission, and the existing diagnostic
`malloc`/`free` calls belong exclusively to the 10 ms group.

Periods, priorities, and static stack allocations are in
[app_rtos.c](Core/Src/app_rtos.c). `vTaskDelayUntil()` uses a common epoch:
first activations occur at 10, 20, and 50 ms after scheduler startup. When due
together, the 10 ms task runs first. Execution time does not accumulate into
the normal release interval. A group that finishes at or after its next deadline
records a miss and waits one full period from completion, avoiding catch-up bursts.

`App_RtosGetStats()` returns activation count, deadline misses, and maximum
elapsed execution time (including preemption, in 1 ms ticks). Sub-millisecond
execution can read as zero. `App_RtosGetStackHeadroom()` returns minimum unused
stack in 32-bit words. Call these APIs from task context, or inspect
`periodic_tasks` in the debugger. Assertions and stack overflow checks stop in
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
from interrupt context. The existing startup test frame is still attempted
before scheduling, with no mailbox busy-wait.

### UDS Service Dispatch

`HAL_CAN_RxFifo0MsgPendingCallback()` (in `app_can.c`) reads exactly one frame and copies its header and eight data bytes into a 16-frame queue. It never parses requests, runs services, transmits, logs, delays, or waits for the FIFO to empty. When the queue is full, it releases the received hardware frame, drops that newest frame, and records an overflow flag and a saturating drop count. Read failures and hardware FIFO overruns are also recorded; the CAN status/error interrupt is enabled.

The 10 ms group removes at most four frames per activation and calls `process_can_frame()`. Adjust `CAN_RX_FRAMES_PER_ACTIVATION` in `app_can.c` only after measuring execution time. This sets a nominal ceiling of 400 dispatched frames/second; the 16-frame queue absorbs short bursts. The existing format uses the first byte as the UDS Service ID (SID); remote frames and frames shorter than two bytes are ignored by the dispatcher. Service handlers and `send_can_message()` run in task context. A short critical section protects each queue removal and each error snapshot, restoring the previous interrupt state before application work. The 50 ms group reports errors and drops at most once per second.

Sustained input faster than processing causes drops. The ISR has fixed work per entry; worst-case timing and stack headroom on the board still need measurement.

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

Validate the linked vectors and CCM stack placement on Windows:

```powershell
powershell -ExecutionPolicy Bypass -File tests/check_rtos_image.ps1
```

On hardware, inspect activation counts over 10 seconds (approximately 1000,
500, and 200); verify zero deadline misses and adequate stack headroom under
CAN traffic. Test CAN bursts, receive overflow, a disconnected bus, and log
queue saturation. Measure GPIO pulses around runnables if sub-millisecond
jitter/WCET is required. A successful build and image check do not establish
real-time timing or validate the existing UDS service implementations.
