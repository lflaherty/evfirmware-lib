/*
 * uart.c
 *
 *  Created on: May 23, 2021
 *      Author: Liam Flaherty
 */

#include "uart.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

REGISTERED_MODULE_STATIC_DEF(UART);

// ------------------- Private data -------------------
static Logging_T* mLog;

static bool isReady = false;

// uart operating info
struct uartInfo {
  // output stream buffer
  bool outputSbEnabled;
  StreamBufferHandle_t outputSb;

  // DMA buffers
  uint8_t uartDmaRx[UART_MAX_DMA_LEN];
  uint8_t uartDmaTx[UART_MAX_DMA_LEN];

  // tx info
  volatile bool txInProgress;
  uint8_t txPendingStorage[UART_MAX_DMA_LEN];
  StaticStreamBuffer_t txPendingStreamStruct;
  StreamBufferHandle_t txPendingStreamHandle;
};

static struct uartInfo usart1;
static struct uartInfo usart3;

/**
 * @brief Get the Uart Dev object for the provided port.
 * 
 * @param huart 
 * @return uartInfo pointer. NULL if invalid port.
 */
static struct uartInfo* getUartDev(const UART_HandleTypeDef* huart)
{
  if (USART1 == huart->Instance) {
    return &usart1;
  } else if (USART3 == huart->Instance) {
    return &usart3;
  } else {
    // uart instance not implemented
    return NULL;
  }
}

/**
 * @brief UART DMA Rx interrupt
 *
 * @brief huart UART handle provided by interrupt
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  if (!isReady) {
    return;
  }

  struct uartInfo* uartDev = getUartDev(huart);
  if (!uartDev) {
    return;
  }

  BaseType_t higherPriorityTaskWoken = pdFALSE;
  if (uartDev->outputSbEnabled) {
    xStreamBufferSendFromISR(uartDev->outputSb, uartDev->uartDmaRx, size,
                             &higherPriorityTaskWoken);
  }

  // start the next DMA transfer
  HAL_UARTEx_ReceiveToIdle_DMA(huart, uartDev->uartDmaRx, UART_MAX_DMA_LEN);

  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

/**
 * @brief UART DMA Tx complete interrupt
 * 
 * @param huart UART handle provided by interrupt
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
{
  if (!isReady) {
    return;
  }

  struct uartInfo* uartDev = getUartDev(huart);
  if (!uartDev) {
    return;
  }

  BaseType_t higherPriorityTaskWoken = pdFALSE;

  // check whether there is more data to transmit
  if (pdTRUE == xStreamBufferIsEmpty(uartDev->txPendingStreamHandle)) {
    uartDev->txInProgress = false;
  } else {
    uint16_t numSending = (uint16_t)xStreamBufferReceiveFromISR(
        uartDev->txPendingStreamHandle, uartDev->uartDmaTx, UART_MAX_DMA_LEN,
        &higherPriorityTaskWoken);

    HAL_StatusTypeDef txStatus = HAL_UART_Transmit_DMA(
        huart, uartDev->uartDmaTx, numSending);
    if (HAL_OK != txStatus) {
      // drop the failed bytes
      uartDev->txInProgress = false;
    }
  }

  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (!isReady) {
    return;
  }

  struct uartInfo* uartDev = getUartDev(huart);
  if (!uartDev) {
    return;
  }

  uartDev->txInProgress = false;

  HAL_UARTEx_ReceiveToIdle_DMA(huart, uartDev->uartDmaRx, UART_MAX_DMA_LEN);
}


// ------------------- Public methods -------------------
UART_Status_T UART_Init(Logging_T* logger)
{
  mLog = logger;
  Log_Print(mLog, "UART_Init begin\n");
  DEPEND_ON(logger, UART_STATUS_ERROR_DEPENDS);

  // USART1
  usart1.txInProgress = false;
  usart1.outputSbEnabled = false;
  usart1.txPendingStreamHandle = xStreamBufferCreateStatic(
      UART_MAX_DMA_LEN,
      1U,
      usart1.txPendingStorage,
      &usart1.txPendingStreamStruct);

  // USART3
  usart3.txInProgress = false;
  usart3.outputSbEnabled = false;
  usart3.txPendingStreamHandle = xStreamBufferCreateStatic(
      UART_MAX_DMA_LEN,
      1U,
      usart3.txPendingStorage,
      &usart3.txPendingStreamStruct);

  isReady = true;

  REGISTER_STATIC(UART, UART_STATUS_ERROR_DEPENDS);
  Log_Print(mLog, "UART_Init complete\n");
  return UART_STATUS_OK;
}

//------------------------------------------------------------------------------
UART_Status_T UART_Config(UART_HandleTypeDef* handle)
{
  Log_Print(mLog, "UART_Config begin\n");
  DEPEND_ON_STATIC(UART, UART_STATUS_ERROR_DEPENDS);

  struct uartInfo* uartDev = getUartDev(handle);
  if (!uartDev) {
    return UART_STATUS_ERROR_NOT_SUPPORTED;
  }

  HAL_UARTEx_ReceiveToIdle_DMA(handle, uartDev->uartDmaRx, UART_MAX_DMA_LEN);

  Log_Print(mLog, "UART_Config complete\n");
  return UART_STATUS_OK;
}

//------------------------------------------------------------------------------
UART_Status_T UART_SetRecvStream(
    const UART_HandleTypeDef* handle,
    const StreamBufferHandle_t sb)
{
  if (!isReady) {
    return UART_STATUS_NOT_READY;
  }

  struct uartInfo* uartDev = getUartDev(handle);
  if (!uartDev) {
    return UART_STATUS_ERROR_NOT_SUPPORTED;
  }

  uartDev->outputSb = sb;
  uartDev->outputSbEnabled = true;

  return UART_STATUS_OK;
}

//------------------------------------------------------------------------------
UART_Status_T UART_SendMessage(UART_HandleTypeDef* handle, uint8_t* data, uint16_t len)
{
  if (!isReady) {
    return UART_STATUS_NOT_READY;
  }

  struct uartInfo* uartDev = getUartDev(handle);
  if (!uartDev) {
    return UART_STATUS_ERROR_NOT_SUPPORTED;
  }

  UART_Status_T ret = UART_STATUS_OK;

  if (uartDev->txInProgress) {
    // append to the stream buffer
    // TODO txInProgress may have a race condition here...
    xStreamBufferSend(uartDev->txPendingStreamHandle, (void*)data, len, pdMS_TO_TICKS(100U));
  } else {
    // directly initiate transfer
    uartDev->txInProgress = true;
    memcpy(uartDev->uartDmaTx, data, len);

    HAL_StatusTypeDef txStatus = HAL_UART_Transmit_DMA(handle, uartDev->uartDmaTx, len);
    if (HAL_OK != txStatus) {
      uartDev->txInProgress = false;
      ret = UART_STATUS_ERROR_TX;
    }
  }

  return ret;
}
