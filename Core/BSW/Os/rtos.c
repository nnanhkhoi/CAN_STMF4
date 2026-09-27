#include "rtos.h"
#include "app_runnables.h"
#include "app_log.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
  const char *name;          /* Human-readable task name used for diagnostics. */
  uint32_t period_ms;        /* Task activation period, in milliseconds. */
  UBaseType_t priority;      /* FreeRTOS task priority. */
  void (*run)(void);         /* Function that executes this task's runnables. */
  StackType_t *stack;        /* Statically allocated task stack buffer. */
  uint32_t stack_words;      /* Number of StackType_t elements in the stack buffer. */
  StaticTask_t tcb;          /* Static FreeRTOS task control block storage. */
  TaskHandle_t handle;       /* Handle returned when the static task is created. */
  App_TaskStats_t stats;     /* Runtime activation and timing statistics. */
} App_PeriodicTask_t;

static StackType_t stack10ms[1024] APP_RTOS_STACK;
static StackType_t stack20ms[256]  APP_RTOS_STACK;
static StackType_t stack50ms[512]  APP_RTOS_STACK;

/* AUTOSAR-style mapping: timing event -> OS task -> ordered runnables.
 * FreeRTOS priorities increase with the number (opposite to NVIC priorities). */
static App_PeriodicTask_t periodic_tasks_st[APP_TASK_COUNT] =
{
  { .name = "Task_10ms", .period_ms = 10U, .priority = 4U,
    .run = App_Runnables10ms, .stack = stack10ms, .stack_words = 1024U },
  { .name = "Task_20ms", .period_ms = 20U, .priority = 3U,
    .run = App_Runnables20ms, .stack = stack20ms, .stack_words = 256U },
  { .name = "Task_50ms", .period_ms = 50U, .priority = 2U,
    .run = App_Runnables50ms, .stack = stack50ms, .stack_words = 512U }
};

static TickType_t schedule_epoch;

/* Run one periodic runnable group and record its activation timing statistics. */
static void PeriodicTask(void *pvParameters)
{
  App_PeriodicTask_t *TaskGroup_st = pvParameters;
  const TickType_t period = pdMS_TO_TICKS(TaskGroup_st->period_ms);
  TickType_t release = schedule_epoch;

  configASSERT(period > 0U);
  for (;;)
  {
    /* First release at epoch + period; no accumulated execution-time drift. */
    vTaskDelayUntil(&release, period);
    TickType_t started = xTaskGetTickCount();
    TaskGroup_st->run();
    TickType_t finished = xTaskGetTickCount();
    TickType_t elapsed = finished - started;

    taskENTER_CRITICAL();
    ++TaskGroup_st->stats.activations;
    if (elapsed > TaskGroup_st->stats.max_execution_ticks)
    {
      TaskGroup_st->stats.max_execution_ticks = elapsed;
    }
    /* Include release latency as well as execution/preemption time. On a
     * missed deadline, restart from now instead of running catch-up bursts. */
    if ((TickType_t)(finished - release) >= period)
    {
      ++TaskGroup_st->stats.deadline_misses;
      release = finished;
    }
    taskEXIT_CRITICAL();
  }
}

/* Create the statically allocated application tasks and start the scheduler. */
void App_RtosStart(void)
{
  uint32_t taskId;

  configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);

  __HAL_RCC_CCMDATARAMEN_CLK_ENABLE();
  
  App_LogInit();

  schedule_epoch = xTaskGetTickCount();

  for (taskId = 0; taskId < APP_TASK_COUNT; ++taskId)
  {
    App_PeriodicTask_t *group = &periodic_tasks_st[taskId];
    /* Create the task statically */
    group->handle = xTaskCreateStatic(PeriodicTask, group->name, group->stack_words, group, group->priority, group->stack, &group->tcb);
    configASSERT(group->handle != NULL);
  }

  vTaskStartScheduler();
  Error_Handler();
}

/* Copy a task's runtime counters while protecting the shared statistics. */
void App_RtosGetStats(App_TaskId_en id, App_TaskStats_t *stats)
{
  configASSERT((uint32_t)id < APP_TASK_COUNT);
  configASSERT(stats != NULL);
  taskENTER_CRITICAL();
  *stats = periodic_tasks_st[id].stats;
  taskEXIT_CRITICAL();
}

/* Return the selected task's stack high-water mark in stack words. */
uint32_t App_RtosGetStackHeadroom(App_TaskId_en id)
{
  configASSERT((uint32_t)id < APP_TASK_COUNT);
  configASSERT(periodic_tasks_st[id].handle != NULL);
  return (uint32_t)uxTaskGetStackHighWaterMark(periodic_tasks_st[id].handle);
}

/* Provide static memory for the FreeRTOS idle task. */
void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack,
                                 uint32_t *stack_words)
{
  static StaticTask_t idle_tcb;
  static StackType_t idle_stack[configMINIMAL_STACK_SIZE] APP_RTOS_STACK;
  *tcb = &idle_tcb;
  *stack = idle_stack;
  *stack_words = configMINIMAL_STACK_SIZE;
}

/* Inspect these symbols in the debugger after an assertion/stack failure. */
const char * volatile app_rtos_assert_file;
volatile uint32_t app_rtos_assert_line;
TaskHandle_t volatile app_rtos_overflow_task;

/* Save assertion details for debugging and halt through the fatal handler. */
void App_RtosAssertFailed(const char *file, uint32_t line)
{
  __disable_irq();
  app_rtos_assert_file = file;
  app_rtos_assert_line = line;
  Error_Handler();
}

/* Record the task whose stack overflowed, then enter the fatal error handler. */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
  (void)name;
  __disable_irq();
  app_rtos_overflow_task = task;
  Error_Handler();
}
