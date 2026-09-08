/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  CAN_RxHeaderTypeDef header;
  uint8_t data[8];
} can_rx_frame_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CAN_RX_QUEUE_CAPACITY 16U
#define CAN_RX_FLAG_OVERFLOW  (1U << 0)
#define CAN_RX_FLAG_READ_ERROR (1U << 1)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* RX ISR produces; foreground consumes with a bounded critical section. */
static can_rx_frame_t can_rx_queue[CAN_RX_QUEUE_CAPACITY];
static uint32_t can_rx_head;
static uint32_t can_rx_tail;
static volatile uint32_t can_rx_count;
static volatile uint32_t can_rx_flags;
static volatile uint32_t can_rx_dropped;
static volatile uint32_t can_error_code;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static uint8_t can_rx_pop(can_rx_frame_t *frame);
static void process_can_frame(can_rx_frame_t *frame);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Copy before releasing the slot, then restore the caller's interrupt state. */
static uint8_t can_rx_pop(can_rx_frame_t *frame)
{
  uint32_t primask = __get_PRIMASK();
  uint8_t available = 0U;

  __disable_irq();
  if (can_rx_count != 0U)
  {
    *frame = can_rx_queue[can_rx_tail];
    can_rx_tail = (can_rx_tail + 1U) % CAN_RX_QUEUE_CAPACITY;
    --can_rx_count;
    available = 1U;
  }
  __set_PRIMASK(primask);
  return available;
}

typedef struct
{
  const char *label;
  uint8_t data[8];
  uint8_t len;
} uds_test_case_t;

static void send_test_can(const uds_test_case_t *tc)
{
  CAN_TxHeaderTypeDef TxHeader;
  uint32_t TxMailbox;
  uint8_t TxData[8] = {0};
  char buf[60];

  memcpy(TxData, tc->data, tc->len > 8 ? 8 : tc->len);

  TxHeader.StdId = 0x7DF;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.DLC = 8;
  TxHeader.TransmitGlobalTime = DISABLE;

  // Wait for a free mailbox
  while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0)
  {
  }

  if (HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox) != HAL_OK)
  {
    snprintf(buf, sizeof(buf), "TX FAIL: %s\r\n", tc->label);
  }
  else
  {
    snprintf(buf, sizeof(buf), "TX OK: %s\r\n", tc->label);
  }
  UART_Send(buf);
  HAL_Delay(10);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  static uint32_t last_error_tick = 0;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN1_Init();
  MX_TIM6_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  UART_Send("UART Initialized Successfully!\r\n");
  const uds_test_case_t test_cases[] = {
    {"Test Case 1", {0x02, 0x10, 0x03}, 3},
    {"Test Case 2", {0x02, 0x10, 0x04}, 3}
  };

  send_test_can(&test_cases[0]);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Process one queued frame per pass so error reporting also gets time. */
    can_rx_frame_t frame;
    if (can_rx_pop(&frame))
    {
      process_can_frame(&frame);
      char buf[120];
      snprintf(buf, sizeof(buf),
        "[RX] ID=0x%03lX DLC=%lu Data=[%02X %02X %02X %02X %02X %02X %02X %02X]\r\n",
        (unsigned long)frame.header.StdId,
        (unsigned long)frame.header.DLC,
        frame.data[0], frame.data[1],
        frame.data[2], frame.data[3],
        frame.data[4], frame.data[5],
        frame.data[6], frame.data[7]);
      UART_Send(buf);
    }

    // Print CAN error if any (with detailed flags)
    if (HAL_GetTick() - last_error_tick >= 1000U)
    {
      last_error_tick = HAL_GetTick();
      uint32_t primask = __get_PRIMASK();
      __disable_irq();
      uint32_t errors = can_error_code;
      uint32_t rx_flags = can_rx_flags;
      uint32_t dropped = can_rx_dropped;
      can_error_code = 0U;
      can_rx_flags = 0U;
      can_rx_dropped = 0U;
      __set_PRIMASK(primask);

      /* Formatting and UART transmission always run with interrupts restored. */
      if (errors != 0U)
      {
        char buf[200];
        snprintf(buf, sizeof(buf), "[CAN ERR] 0x%08lX | Flags: %s%s%s%s%s%s%s%s%s%s\r\n",
          (unsigned long)errors,
          (errors & 0x01) ? "EWG " : "",      // Protocol Error Warning
          (errors & 0x02) ? "EPV " : "",      // Error Passive
          (errors & 0x04) ? "BOF " : "",      // Bus-off
          (errors & 0x08) ? "STF " : "",      // Stuff error
          (errors & 0x10) ? "FOR " : "",      // Form error
          (errors & 0x20) ? "ACK " : "",      // Acknowledgment error
          (errors & 0x40) ? "BR " : "",       // Bit recessive error
          (errors & 0x80) ? "BD " : "",       // Bit dominant error
          (errors & 0x100) ? "CRC " : "",     // CRC error
          (errors & HAL_CAN_ERROR_RX_FOV0) ? "RX_FOV0 " : ""
        );
        UART_Send(buf);
        }
      if (rx_flags != 0U)
      {
        char buf[120];
        snprintf(buf, sizeof(buf), "[CAN RX] Flags: %s%s dropped=%lu\r\n",
          (rx_flags & CAN_RX_FLAG_OVERFLOW) ? "QUEUE_OVERFLOW " : "",
          (rx_flags & CAN_RX_FLAG_READ_ERROR) ? "READ_ERROR " : "",
          (unsigned long)dropped);
        UART_Send(buf);
      }
    }


  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 64;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/* Function to send messages via UART */
void UART_Send(const char *message)
{
  HAL_UART_Transmit(&huart3, (uint8_t *)message, strlen(message), HAL_MAX_DELAY);
}

/* Interrupt Callbacks */

/**
 * @brief  Rx FIFO 0 message pending callback.
 * @param  hcan pointer to a CAN_HandleTypeDef structure that contains
 *         the configuration information for the specified CAN.
 * @retval None
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  can_rx_frame_t frame = {0};

  /* Exactly one read per callback. Never wait for, or drain, the FIFO. */
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &frame.header, frame.data) != HAL_OK)
  {
    can_rx_flags |= CAN_RX_FLAG_READ_ERROR;
    can_error_code |= HAL_CAN_GetError(hcan);
    return;
  }

  /* Read first so a full software queue still releases the hardware FIFO. */
  if (can_rx_count == CAN_RX_QUEUE_CAPACITY)
  {
    can_rx_flags |= CAN_RX_FLAG_OVERFLOW;
    if (can_rx_dropped != UINT32_MAX)
    {
      ++can_rx_dropped;
    }
    return;
  }

  can_rx_queue[can_rx_head] = frame;
  can_rx_head = (can_rx_head + 1U) % CAN_RX_QUEUE_CAPACITY;
  __DMB();
  ++can_rx_count;
}

/* Foreground only: parsing, service execution and response transmission. */
static void process_can_frame(can_rx_frame_t *frame)
{
  uint8_t *rcvd_msg = frame->data;
  uint8_t response[3];

  /* The existing dispatcher expects a SID and at least one parameter byte. */
  if (frame->header.RTR != CAN_RTR_DATA ||
      frame->header.DLC < 2U || frame->header.DLC > sizeof(frame->data))
  {
    return;
  }

  switch (rcvd_msg[0])
  {
  case UDS_DIAGNOSTIC_SESSION_CONTROL:
    // Call the Diagnostic Session Control service handler
    uds_diagnostic_session_control(rcvd_msg[1]);
    break;

  case UDS_ECU_RESET:
    // Call the ECU Reset service handler with resetType
    uds_ecu_reset(rcvd_msg[1]);
    break;
  case UDS_SECURITY_ACCESS:
    // Call the Security Access service handler
    uds_security_access(rcvd_msg[1], &rcvd_msg[2], frame->header.DLC - 2);
    break;
  case UDS_COMMUNICATION_CONTROL:
    uds_communication_control(rcvd_msg[1]);
    break;
  case UDS_TESTER_PRESENT:
    // Call the TesterPresent service handler
    uds_tester_present(rcvd_msg[1]);
    break;
  case UDS_ACCESS_TIMING_PARAMETER:
    // Call the Access Timing Parameter service handler
    uds_access_timing_parameter(rcvd_msg[1], &rcvd_msg[2], frame->header.DLC - 2);
    break;
  case UDS_SECURED_DATA_TRANSMISSION:
    // Call the Secured Data Transmission service handler
    uds_secured_data_transmission(&rcvd_msg[1], frame->header.DLC - 1);
    break;
  case UDS_CONTROL_DTC_SETTING:
    // Call the ControlDTCSetting service handler
    uds_control_dtc_setting(rcvd_msg[1]);
    break;
  case UDS_RESPONSE_ON_EVENT:
    // Call the ResponseOnEvent service handler
    uds_response_on_event(rcvd_msg[1], &rcvd_msg[2], frame->header.DLC - 2);
    break;
  case UDS_LINK_CONTROL:
    // Call the LinkControl service handler
    uds_link_control(rcvd_msg[1], &rcvd_msg[2], frame->header.DLC - 2);
    break;
  case UDS_READ_DATA_BY_IDENTIFIER:
    // Call the ReadDataByIdentifier service handler
    uds_read_data_by_identifier(&rcvd_msg[1], frame->header.DLC - 1);
    break;
  case UDS_READ_DATA_BY_PERIODIC_IDENTIFIER:
    // Call the ReadDataByPeriodicIdentifier service handler
    uds_read_data_by_periodic_identifier(&rcvd_msg[1], frame->header.DLC - 1);
    break;
  case UDS_DYNAMICAL_DEFINE_DATA_IDENTIFIER:
    // Call the DynamicallyDefineDataIdentifier service handler
    uds_dynamically_define_data_identifier(rcvd_msg[1], &rcvd_msg[2], frame->header.DLC - 2);
    break;
  case UDS_WRITE_DATA_BY_IDENTIFIER:
    // Call the WriteDataByIdentifier service handler
    uds_write_data_by_identifier(&rcvd_msg[1], frame->header.DLC - 1);
    break;
  case UDS_CLEAR_DIAGNOSTIC_INFORMATION:
    // Call the ClearDiagnosticInformation service handler
    uds_clear_diagnostic_information(&rcvd_msg[1], frame->header.DLC - 1);
    break;
  case UDS_READ_DTC_INFORMATION:
    // Appeler la fonction pour gérer le service ReadDTCInformation
    uds_read_dtc_information(rcvd_msg[1], &rcvd_msg[2], frame->header.DLC - 2);
    break;
  case UDS_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER:
    uds_input_output_control_by_identifier((IOControlRequest_t *)&rcvd_msg[1], NULL);
    break;
  case UDS_ROUTINE_CONTROL:
    uds_routine_control((RoutineControlRequest_t *)&rcvd_msg[1], NULL);
    break;
  case UDS_REQUEST_DOWNLOAD:
    uds_request_download((RequestDownload_t *)&rcvd_msg[1]);
    break;
  case UDS_REQUEST_UPLOAD:
    uds_request_upload((RequestUpload_t *)&rcvd_msg[1]);
    break;
  case UDS_TRANSFER_DATA:
    uds_transfer_data((RequestTransferData_t *)&rcvd_msg[1]);
    break;
  case UDS_REQUEST_TRANSFER_EXIT:
    uds_request_transfer_exit((RequestTransferExit_t *)&rcvd_msg[1], NULL);
    break;
  case UDS_REQUEST_FILE_TRANSFER:
    uds_request_file_transfer((RequestFileTransfer_t *)&rcvd_msg[1]);
    break;

  default:
    // Remplir le message de réponse négative pour un service non supporté
    response[0] = UDS_NEGATIVE_RESPONSE;     // Réponse négative générique
    response[1] = rcvd_msg[0];               // Service non supporté
    response[2] = NRC_SERVICE_NOT_SUPPORTED; // Code NRC (ServiceNotSupported)

    // Envoyer le message de réponse négative via CAN
    send_can_message(response, 3);
    break;
  }
}
/**
 * @brief  Transmission Mailbox 0 complete callback.
 * @param  hcan pointer to a CAN_HandleTypeDef structure that contains
 *         the configuration information for the specified CAN.
 * @retval None
 */
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
  // TX complete — flag could be added here too if needed
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
  can_error_code |= HAL_CAN_GetError(hcan);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
