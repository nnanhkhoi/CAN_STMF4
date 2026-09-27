#ifndef TEST_CAN_H
#define TEST_CAN_H
#include <stdint.h>

/* Minimal bxCAN/HAL surface for testing the production mailbox adapter on a PC. */
typedef struct { uint32_t TSR; uint32_t ESR; } CAN_TypeDef;
typedef struct { CAN_TypeDef *Instance; } CAN_HandleTypeDef;
typedef struct
{
  uint32_t StdId, ExtId, IDE, RTR, DLC, TransmitGlobalTime;
} CAN_TxHeaderTypeDef;
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;
#define CAN_TX_MAILBOX0 1U
#define CAN_TX_MAILBOX1 2U
#define CAN_TX_MAILBOX2 4U
#define CAN_TSR_RQCP0 1U
#define CAN_TSR_TXOK0 2U
#define CAN_ESR_BOFF 4U
#define CAN_ID_STD 0U
#define CAN_RTR_DATA 0U
#define DISABLE 0U
extern CAN_HandleTypeDef hcan1;
uint32_t HAL_CAN_IsTxMessagePending(CAN_HandleTypeDef *handle, uint32_t mailbox);
uint32_t HAL_CAN_GetTxMailboxesFreeLevel(CAN_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *handle, CAN_TxHeaderTypeDef *header,
                                     uint8_t *data, uint32_t *mailbox);
HAL_StatusTypeDef HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *handle, uint32_t mailbox);
#endif
