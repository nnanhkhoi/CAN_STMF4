#ifndef RTOS_H
#define RTOS_H

#include <stdint.h>

/* CPU-only stacks in CCM, initialized by xTaskCreateStatic before use.
 * Never pass a buffer on one of these stacks to a DMA peripheral. */
#define APP_RTOS_STACK __attribute__((section(".rtos_stacks"), aligned(8)))

typedef enum
{
  APP_TASK_10MS = 0,
  APP_TASK_20MS,
  APP_TASK_50MS,
  APP_TASK_COUNT
} App_TaskId_en;

typedef struct
{
  uint32_t activations;          /* Number of times the task has run. */
  uint32_t deadline_misses;      /* Activations that started after their period deadline. */
  uint32_t max_execution_ticks;  /* Longest measured run time, in RTOS ticks. */
} App_TaskStats_t;

/* Call once after all HAL/peripheral initialization. Does not return. */
void App_RtosStart(void);
/* Task-context snapshot. Stack high-water marks are in 32-bit words. */
void App_RtosGetStats(App_TaskId_en id, App_TaskStats_t *stats);
uint32_t App_RtosGetStackHeadroom(App_TaskId_en id);

#endif
