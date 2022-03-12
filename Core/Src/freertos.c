/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "usbd_customhid.h"
#include "usb_msg_queue.h"
#include "common.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for LedTask */
osThreadId_t LedTaskHandle;
const osThreadAttr_t LedTask_attributes = {
  .name = "LedTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t usb_task_handle;
const osThreadAttr_t usb_task_attributes = {
  .name = "usb_task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void led_task(void *argument);
void usb_task(void *argument);

extern void MX_USB_DEVICE_Init(void);
extern USBD_HandleTypeDef hUsbDeviceFS;
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of LedTask */
  LedTaskHandle = osThreadNew(led_task, NULL, &LedTask_attributes);
  usb_task_handle = osThreadNew(usb_task, NULL, &usb_task_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_led_task */
/**
  * @brief  Function implementing the LedTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_led_task */
void led_task(void *argument)
{
    for(;;) { 
        cmd_packet packet;

        osDelay(20);
        packet.cmd.bit.type = INTF_CMD_TYPE_GPIO;
        packet.cmd.bit.dir = INTF_CMD_DIR_OUT;
        packet.gpio.bit.group = CHIP_GPIOC;
        packet.gpio.bit.pin = 13;
        packet.data_len = 1;
        packet.data[0] = 0;
        usb_msg_queue_put(&packet);
        //HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

        osDelay(20);
        packet.data[0] = 1;
        usb_msg_queue_put(&packet);
        //HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        //USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, g_usb_receive_buffer, sizeof(g_usb_receive_buffer));
        //memset(g_usb_receive_buffer, 0, sizeof(g_usb_receive_buffer));
    }
}

cmd_packet g_test_packet;

void usb_task(void *argument)
{
    int ret;

    MX_USB_DEVICE_Init();
    log_err("usb init ok\n");

    ret = usb_msg_queue_init();
    if (ret < 0) {
        log_err("usb_msg_queue_init failed\n");
        goto exit;
    }

    g_test_packet.cmd.bit.type = 1;
    g_test_packet.cmd.bit.dir = 1;
    g_test_packet.gpio.bit.group = 3;
    g_test_packet.gpio.bit.pin = 4;
    g_test_packet.data_len = 1;
    g_test_packet.data[0] = 1;
    usb_msg_queue_put(&g_test_packet);

    for(;;)
    {
        cmd_packet packet;
        int ret;

        ret = usb_msg_queue_get(&packet);
        if (ret == USB_MSG_RETRY) {
        continue;
        } else if (ret == USB_MSG_FAILED) {
        log_err("usb_msg_queue_get failed\n");
        break;
        }
        
        log_info("get packet: [cmd: %d, dir: %d, group: %d, pin: %d, len: %d, value: %d]\n", packet.cmd.bit.type,
            packet.cmd.bit.dir, packet.gpio.bit.group, packet.gpio.bit.pin, packet.data_len, packet.data[0]);
        ret = msg_parse_exec(&packet);
        if (ret != USB_MSG_OK) {
        log_err("msg_parse_exec failed\n");
        continue;
        }
        /* TODO: parse, exec and report result */
        //USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, g_usb_receive_buffer, sizeof(g_usb_receive_buffer));
    }

exit:
    usb_msg_queue_deinit();
    /* USER CODE END led_task */
    while (1) {};
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
