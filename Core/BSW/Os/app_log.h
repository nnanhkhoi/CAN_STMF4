#ifndef APP_LOG_H
#define APP_LOG_H

#include <stdint.h>

/* Internal startup API: create the static queue/worker before the scheduler. */
void App_LogInit(void);
/* Task context only. Copies the string; drops new messages if the queue is full. */
void UART_Send(const char *message);
uint32_t App_LogGetDroppedCount(void);
uint32_t App_LogGetErrorCount(void);

#endif
