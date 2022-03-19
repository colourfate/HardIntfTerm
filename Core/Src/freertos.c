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
#include "cmsis_os2.h"
#include "usbd_customhid.h"
#include "usb_msg_queue.h"
#include "common.h"
#include "usart.h"

osThreadId_t usb_task_handle;
const osThreadAttr_t usb_task_attributes = {
  .name = "usb_task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

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
    usb_task_handle = osThreadNew(usb_task, NULL, &usb_task_attributes);
    test_case_init();
}

void usb_task(void *argument)
{
    int ret;
    cmd_packet *packet;

    MX_USB_DEVICE_Init();
    log_err("usb init ok\n");

    packet = malloc(INTF_PROTOCOL_PACKET_MAX);

    ret = usb_msg_queue_init();
    if (ret < 0) {
        log_err("usb_msg_queue_init failed\n");
        goto exit;
    }

    for(;;)
    {
        int ret;

        ret = usb_msg_queue_block_get(packet);
        if (ret == USB_MSG_FAILED) {
            log_err("usb_msg_queue_get failed\n");
            break;
        }
        
        //log_info("get packet: [cmd: %d, dir: %d, group: %d, pin: %d, len: %d, value: %d]\n", packet->cmd.bit.type,
        //    packet->cmd.bit.dir, packet->gpio.bit.group, packet->gpio.bit.pin, packet->data_len, packet->data[0]);
        ret = msg_parse_exec(packet);
        if (ret != USB_MSG_OK) {
            log_err("msg_parse_exec failed\n");
            continue;
        }

        if (packet->cmd.bit.dir == INTF_CMD_DIR_IN) {
            int i;
            log_info("put packet: [cmd: %d, dir: %d, group: %d, pin: %d, len: %d, value: %d]\n", packet->cmd.bit.type,
                packet->cmd.bit.dir, packet->gpio.bit.group, packet->gpio.bit.pin, packet->data_len, packet->data[0]);
            for (i = 0; i < packet->data_len; i++) {
                printf("%c", packet->data[i]);
            }
            printf("\n");
        }
        /* TODO: parse, exec and report result */
        //USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, g_usb_receive_buffer, sizeof(g_usb_receive_buffer));
    }

exit:
    free(packet);
    usb_msg_queue_deinit();
    /* USER CODE END led_task */
    while (1) {};
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
