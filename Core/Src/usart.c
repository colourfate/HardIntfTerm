/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usart.h"
#include "cmsis_os2.h"
#include "common.h"

#define UART_RX_MAX_LEN 128

typedef struct {
    uint8_t uart_num;
    uint8_t rx_temp;
    osMessageQueueId_t queue;
    UART_HandleTypeDef *uart_def ;
} uart_context;

#ifdef STM32F411xE
#define MAX_UART_PORT_NUM 2

/* Adapt to stm32 hal library */
UART_HandleTypeDef huart1 = {.Instance = USART1};
UART_HandleTypeDef huart2 = {.Instance = USART2};

static uart_context g_uart_ctx_tab[MAX_UART_PORT_NUM] = {
    { .uart_num = 1, .uart_def = &huart1 },
    { .uart_num = 2, .uart_def = &huart2 },
};
#else
#error No define chip support uart context
#endif

static inline uart_context *find_uart_ctx(int uart_num)
{
    int i;

    for (i = 0; i < MAX_UART_PORT_NUM; i++) {
        if (g_uart_ctx_tab[i].uart_num == uart_num) {
            return &g_uart_ctx_tab[i];
        }
    }

    return NULL;
}

static inline uart_context *find_uart_ctx_by_def(UART_HandleTypeDef *uart_def)
{
    int i;

    for (i = 0; i < MAX_UART_PORT_NUM; i++) {
        if (g_uart_ctx_tab[i].uart_def == uart_def) {
            return &g_uart_ctx_tab[i];
        }
    }

    return NULL;
}

int serial_init(int uart_num, UART_InitTypeDef *hal_uart_init)
{
    uart_context *uart_ctx;

    if (hal_uart_init == NULL) {
        return osError;
    }

    uart_ctx = find_uart_ctx(uart_num);
    if (uart_ctx == NULL) {
        return osError;
    }

    uart_ctx->uart_def->Init = *hal_uart_init;
    if (HAL_UART_Init(uart_ctx->uart_def) != HAL_OK) {
        return osError;
    }

    HAL_UART_Receive_IT(&uart_ctx->uart_def->Instance, &uart_ctx->rx_temp, 1);
    uart_ctx->queue = osMessageQueueNew(UART_RX_MAX_LEN, 1, NULL);
    if (uart_ctx->queue == NULL) {
        HAL_UART_DeInit(uart_ctx->uart_def);
        return osError;
    }

    return osOK;
}

/*
void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_UART_Receive_IT(&huart2, &g_rx_byte, 1);

    g_rx_queue[1] = osMessageQueueNew(UART_RX_MAX_LEN, 1, NULL);
    if (g_rx_queue[1] == NULL) {
        log_err("osMessageQueueNew failed\n");
        Error_Handler();
    }
}
*/

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (uartHandle->Instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        /**USART2 GPIO Configuration
        PA2     ------> USART2_TX
        PA3     ------> USART2_RX
        */
        GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* USART2 interrupt Init */
        HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART2_IRQn);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{
  if(uartHandle->Instance==USART2)
  {
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2|GPIO_PIN_3);
    HAL_NVIC_DisableIRQ(USART2_IRQn);
  }
}

/* USER CODE BEGIN 1 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uart_context *uart_ctx = find_uart_ctx_by_def(huart);
    if (uart_ctx == NULL) {
        return;
    }

    (void)osMessageQueuePut(uart_ctx->queue, &uart_ctx->rx_temp, 0, 0);
    HAL_UART_Receive_IT(huart, &uart_ctx->rx_temp, 1);
}

int uart_transmit(uint8_t uart_num, uint8_t *data, int len)
{
    uart_context *uart_ctx = find_uart_ctx(uart_num);
    if (uart_ctx == NULL) {
        return osError;
    }

    if (data == NULL) {
        return osError;
    }

    HAL_UART_Transmit(uart_ctx->uart_def, data, len, 0xFFFF);

    return osOK;
}

uint32_t read_uart_rx_buffer(uint8_t uart_num, uint8_t *data, uint8_t len)
{
    uint8_t i;
    int ret;
    uart_context *uart_ctx = find_uart_ctx(uart_num);

    if (uart_ctx == NULL) {
        log_err("support uart num: %d\n", uart_num);
        return osError;
    }

    if (data == NULL || len > UART_RX_MAX_LEN) {
        log_err("invalid param: %p, %d\n", data, len);
        return 0;
    }

    if (uart_ctx->queue == NULL) {
        log_err("rx queue not init\n");
        return 0;
    }

    for (i = 0; i < len; i++) {
        uint8_t byte;

        ret = osMessageQueueGet(uart_ctx->queue, &byte, NULL, 0);
        if (ret == osErrorResource) {
            break;
        }
        data[i] = byte;
    }

    return i;
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
