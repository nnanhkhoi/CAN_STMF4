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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
CAN_RxHeaderTypeDef RxHeader;

/* Debug buffer for CAN RX — filled in ISR, printed in main loop */
volatile uint8_t can_rx_flag = 0;
uint8_t can_rx_debug_data[8];
CAN_RxHeaderTypeDef can_rx_debug_header;
volatile uint32_t can_error_code = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN1_Init(void);
static void MX_TIM6_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
static void UART_Send(const char *message);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
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

  // Attendre qu'une mailbox soit libre
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

  // Configurer le filtre CAN — accept ALL IDs for debugging
  CAN_FilterTypeDef canfilterconfig;
  canfilterconfig.FilterActivation = CAN_FILTER_ENABLE;
  canfilterconfig.FilterBank = 0;
  canfilterconfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  canfilterconfig.FilterIdHigh = 0x0000;
  canfilterconfig.FilterIdLow = 0x0000;
  canfilterconfig.FilterMaskIdHigh = 0x0000;   // Mask=0 => accept all IDs
  canfilterconfig.FilterMaskIdLow = 0x0000;
  canfilterconfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canfilterconfig.FilterScale = CAN_FILTERSCALE_32BIT;
  canfilterconfig.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig) != HAL_OK)
  {
    UART_Send("ERR: CAN filter config failed\r\n");
    Error_Handler();
  }
  UART_Send("CAN filter configured (accept all IDs)\r\n");

  // Activer les notifications pour les interruptions CAN (RX + TX + errors)
  if (HAL_CAN_ActivateNotification(&hcan1,
        CAN_IT_RX_FIFO0_MSG_PENDING |
        CAN_IT_TX_MAILBOX_EMPTY |
        CAN_IT_ERROR_WARNING |
        CAN_IT_ERROR_PASSIVE |
        CAN_IT_BUSOFF |
        CAN_IT_LAST_ERROR_CODE |
        CAN_IT_ERROR) != HAL_OK)
  {
    UART_Send("ERR: CAN notification activation failed\r\n");
    Error_Handler();
  }
  UART_Send("CAN notifications activated\r\n");

  // Démarrer le module CAN
  if (HAL_CAN_Start(&hcan1) != HAL_OK)
  {
    UART_Send("ERR: CAN start failed\r\n");
    Error_Handler();
  }
  UART_Send("CAN started in Normal mode — waiting for external messages...\r\n");

  uint32_t tx_counter = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // Cyclic debug TX every 1 second
    {
      CAN_TxHeaderTypeDef TxHeader;
      uint32_t TxMailbox;
      uint8_t TxData[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00};
      char buf[100];

      // Put counter in bytes 4-7 so you can see it increment
      TxData[4] = (uint8_t)(tx_counter >> 24);
      TxData[5] = (uint8_t)(tx_counter >> 16);
      TxData[6] = (uint8_t)(tx_counter >> 8);
      TxData[7] = (uint8_t)(tx_counter);

      TxHeader.StdId = 0x123;
      TxHeader.IDE = CAN_ID_STD;
      TxHeader.RTR = CAN_RTR_DATA;
      TxHeader.DLC = 8;
      TxHeader.TransmitGlobalTime = DISABLE;

      uint32_t free = HAL_CAN_GetTxMailboxesFreeLevel(&hcan1);
      if (free > 0)
      {
        HAL_StatusTypeDef status = HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox);
        snprintf(buf, sizeof(buf),
          "[TX #%lu] ID=0x123 status=%s mailbox=%lu free=%lu\r\n",
          (unsigned long)tx_counter,
          (status == HAL_OK) ? "OK" : "FAIL",
          (unsigned long)TxMailbox,
          (unsigned long)free);
      }
      else
      {
        snprintf(buf, sizeof(buf),
          "[TX #%lu] SKIPPED — no free mailbox (stuck, bus-off?)\r\n",
          (unsigned long)tx_counter);
      }
      UART_Send(buf);
      tx_counter++;
    }

    // Print CAN RX debug info outside ISR (safe UART call)
    if (can_rx_flag)
    {
      can_rx_flag = 0;
      char buf[120];
      snprintf(buf, sizeof(buf),
        "[RX] ID=0x%03lX DLC=%lu Data=[%02X %02X %02X %02X %02X %02X %02X %02X]\r\n",
        (unsigned long)can_rx_debug_header.StdId,
        (unsigned long)can_rx_debug_header.DLC,
        can_rx_debug_data[0], can_rx_debug_data[1],
        can_rx_debug_data[2], can_rx_debug_data[3],
        can_rx_debug_data[4], can_rx_debug_data[5],
        can_rx_debug_data[6], can_rx_debug_data[7]);
      UART_Send(buf);
    }

    // Print CAN error if any
    if (can_error_code)
    {
      char buf[80];
      snprintf(buf, sizeof(buf), "[CAN ERR] ErrorCode=0x%08lX\r\n",
        (unsigned long)can_error_code);
      UART_Send(buf);
      can_error_code = 0;
    }

    HAL_Delay(1000);
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
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV6;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 12;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_2TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = ENABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 65535;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
  uint8_t rcvd_msg[8];
  uint8_t response[3];
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, rcvd_msg) == HAL_OK)
  {
    // Copy to debug buffer — will be printed in main loop (safe context)
    memcpy(can_rx_debug_data, rcvd_msg, 8);
    memcpy((void *)&can_rx_debug_header, &RxHeader, sizeof(CAN_RxHeaderTypeDef));
    can_rx_flag = 1;

    // Identifier le service UDS basé sur le premier octet du message
    switch (rcvd_msg[0])
    {
    case UDS_DIAGNOSTIC_SESSION_CONTROL:
      // Appeler la fonction pour le service Diagnostic Session Control
      uds_diagnostic_session_control(rcvd_msg[1]);
      break;

    case UDS_ECU_RESET:
      // Appeler la fonction pour le service ECU Reset avec le resetType
      uds_ecu_reset(rcvd_msg[1]);
      break;
    case UDS_SECURITY_ACCESS:
      // Appeler directement la fonction pour le service Security Access
      uds_security_access(rcvd_msg[1], &rcvd_msg[2], RxHeader.DLC - 2);
      break;
    case UDS_COMMUNICATION_CONTROL:
      uds_communication_control(rcvd_msg[1]);
      break;
    case UDS_TESTER_PRESENT:
      // Appeler la fonction pour le service TesterPresent
      uds_tester_present(rcvd_msg[1]);
      break;
    case UDS_ACCESS_TIMING_PARAMETER:
      // Appeler la fonction pour le service Access Timing Parameter
      uds_access_timing_parameter(rcvd_msg[1], &rcvd_msg[2], RxHeader.DLC - 2);
      break;
    case UDS_SECURED_DATA_TRANSMISSION:
      // Appeler la fonction pour le service Secured Data Transmission
      uds_secured_data_transmission(&rcvd_msg[1], RxHeader.DLC - 1);
      break;
    case UDS_CONTROL_DTC_SETTING:
      // Appeler la fonction pour le service ControlDTCSetting
      uds_control_dtc_setting(rcvd_msg[1]);
      break;
    case UDS_RESPONSE_ON_EVENT:
      // Appeler la fonction pour le service ResponseOnEvent
      uds_response_on_event(rcvd_msg[1], &rcvd_msg[2], RxHeader.DLC - 2);
      break;
    case UDS_LINK_CONTROL:
      // Appeler la fonction pour le service LinkControl
      uds_link_control(rcvd_msg[1], &rcvd_msg[2], RxHeader.DLC - 2);
      break;
    case UDS_READ_DATA_BY_IDENTIFIER:
      // Appeler la fonction pour le service ReadDataByIdentifier
      uds_read_data_by_identifier(&rcvd_msg[1], RxHeader.DLC - 1);
      break;
    case UDS_READ_DATA_BY_PERIODIC_IDENTIFIER:
      // Appeler la fonction pour le service ReadDataByPeriodicIdentifier
      uds_read_data_by_periodic_identifier(&rcvd_msg[1], RxHeader.DLC - 1);
      break;
    case UDS_DYNAMICAL_DEFINE_DATA_IDENTIFIER:
      // Appeler la fonction pour le service DynamicallyDefineDataIdentifier
      uds_dynamically_define_data_identifier(rcvd_msg[1], &rcvd_msg[2], RxHeader.DLC - 2);
      break;
    case UDS_WRITE_DATA_BY_IDENTIFIER:
      // Appeler la fonction pour le service WriteDataByIdentifier
      uds_write_data_by_identifier(&rcvd_msg[1], RxHeader.DLC - 1);
      break;
    case UDS_CLEAR_DIAGNOSTIC_INFORMATION:
      // Appeler la fonction pour le service ClearDiagnosticInformation
      uds_clear_diagnostic_information(&rcvd_msg[1], RxHeader.DLC - 1);
      break;
    case UDS_READ_DTC_INFORMATION:
      // Appeler la fonction pour gérer le service ReadDTCInformation
      uds_read_dtc_information(rcvd_msg[1], &rcvd_msg[2], RxHeader.DLC - 2);
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
  can_error_code = HAL_CAN_GetError(hcan);
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
