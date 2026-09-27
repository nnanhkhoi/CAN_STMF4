#ifndef TEST_STM32_HAL_H
#define TEST_STM32_HAL_H
#include "can.h"
#define PWR_MAINREGULATOR_ON 0U
#define PWR_SLEEPENTRY_WFI 0U
/* Only these platform services are used by the real UDS modules under test. */
uint32_t HAL_GetTick(void);
void NVIC_SystemReset(void);
void HAL_PWR_EnterSTOPMode(uint32_t regulator, uint8_t entry);
#endif
