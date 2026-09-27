#include "app_log.h"
#include "rtos.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "usart.h"
#include <string.h>

#define LOG_QUEUE_LENGTH 16U
#define LOG_MESSAGE_SIZE 224U
#define LOG_UART_TIMEOUT_MS 50U

typedef struct
{
  uint16_t length;              /* Number of valid characters in text, excluding the terminator. */
  char text[LOG_MESSAGE_SIZE];  /* Null-terminated message queued for UART output. */
} LogMessage_t;

static StaticQueue_t log_queue_control;
static uint8_t log_queue_storage[LOG_QUEUE_LENGTH * sizeof(LogMessage_t)];
static QueueHandle_t log_queue;
static StaticTask_t log_task_control;
static StackType_t log_stack[256] APP_RTOS_STACK;
static volatile uint32_t log_dropped_u32;
static volatile uint32_t log_errors_u32;

/* Drain queued log messages and transmit them over UART. */
static void LogTask(void *pvParameters)
{
  LogMessage_t message;
  (void)pvParameters;

  for (;;)
  {
    if (xQueueReceive(log_queue, &message, portMAX_DELAY) == pdPASS)
    {
      /* Only this task accesses UART after scheduler startup. */
      if (HAL_UART_Transmit(&huart3, (uint8_t *)message.text, message.length, LOG_UART_TIMEOUT_MS) != HAL_OK)
      {
        if (log_errors_u32 != UINT32_MAX)
        {
          ++log_errors_u32;
        }
      }
    }
  }
}

/* Create the static message queue and its dedicated UART worker task. */
void App_LogInit(void)
{
  configASSERT(log_queue == NULL);
  log_queue = xQueueCreateStatic(LOG_QUEUE_LENGTH, sizeof(LogMessage_t), log_queue_storage, &log_queue_control);
  configASSERT(log_queue != NULL);

  TaskHandle_t handle = xTaskCreateStatic(LogTask, "UART_Log", 256U, NULL, 1U, log_stack, &log_task_control);

  configASSERT(handle != NULL);
}

/* Queue a copy of a message for UART output, dropping it if the queue is full. */
void UART_Send(const char *text)
{
  LogMessage_t LogMessage_st = {0};
  
  if (text == NULL)
  {
    return;
  }

  /* Existing boot messages are sent before any kernel objects are created. */
  if (log_queue == NULL)
  {
    (void)HAL_UART_Transmit(&huart3, (uint8_t *)text, (uint16_t)strlen(text), LOG_UART_TIMEOUT_MS);
    return;
  }

  while (LogMessage_st.length < LOG_MESSAGE_SIZE - 1U && text[LogMessage_st.length] != '\0')
  {
    LogMessage_st.text[LogMessage_st.length] = text[LogMessage_st.length];
    ++LogMessage_st.length;
  }

  if (xQueueSend(log_queue, &LogMessage_st, 0U) != pdPASS)
  {
    taskENTER_CRITICAL();
    if (log_dropped_u32 != UINT32_MAX)
    {
      ++log_dropped_u32;
    }
    taskEXIT_CRITICAL();
  }
}

/* Return the count of log messages rejected because the queue was full. */
uint32_t App_LogGetDroppedCount(void)
{
  return log_dropped_u32;
}

/* Return the number of UART transmission failures recorded by the log task. */
uint32_t App_LogGetErrorCount(void)
{
  return log_errors_u32;
}
