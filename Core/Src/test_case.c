#include "test_case.h"
#include "FreeRTOS.h"
#include <string.h>
#include "usb_msg_queue.h"
#include "common.h"
#include "cmsis_os2.h"

const osThreadAttr_t gpio_task_attributes = {
  .name = "gpio_task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t serail_in_task_attributes = {
  .name = "serail_in_task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t serail_out_task_attributes = {
  .name = "serail_out_task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/*
 * Description: write gpio and read gpio
 * Expect: LED flash and usb task read button press
 */
void gpio_task(void *argument)
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

        osDelay(20);
        packet.data[0] = 1;
        usb_msg_queue_put(&packet);

        packet.cmd.bit.type = INTF_CMD_TYPE_GPIO;
        packet.cmd.bit.dir = INTF_CMD_DIR_IN;
        packet.gpio.bit.group = CHIP_GPIOA;
        packet.gpio.bit.pin = 0;
        packet.data_len = 1;
        packet.data[0] = 0;
        usb_msg_queue_put(&packet);
    }
}

/*
 * Description: Uart write string
 * Expect: Uart2 print string
 */
void serail_out_task(void *argument)
{
    cmd_packet *packet;
    char *str = "uart testdddddddddddddddddddddddddddddddddddd\r\n";

    packet = malloc(sizeof(cmd_packet) + strlen(str) - 1);
    packet->cmd.bit.type = INTF_CMD_TYPE_SERIAL;
    packet->cmd.bit.dir = INTF_CMD_DIR_OUT;
    packet->gpio.bit.group = CHIP_MUL_FUNC;
    packet->gpio.bit.pin = 2;
    packet->data_len = strlen(str);
    strcpy((char *)packet->data, str);
    osDelay(1000);

    for (;;) {
        usb_msg_queue_put(packet);
        osDelay(100);
    }

}

/*
 * Description: Uart read string once a sencond, 10 bytes each time 
 * Expect: usb task get string
 */
void serail_in_task(void *argument)
{
    cmd_packet *packet;
    const uint8_t len = 10;

    packet = malloc(sizeof(cmd_packet) + len - 1);
    packet->cmd.bit.type = INTF_CMD_TYPE_SERIAL;
    packet->cmd.bit.dir = INTF_CMD_DIR_IN;
    packet->gpio.bit.group = CHIP_MUL_FUNC;
    packet->gpio.bit.pin = 2;
    packet->data_len = len;
    osDelay(1000);

    for (;;) {
        usb_msg_queue_put(packet);
        osDelay(1000);
    }

    free(packet);
}

void test_case_init(void)
{
    //osThreadNew(serail_in_task, NULL, &serail_in_task_attributes);
    //osThreadNew(serail_out_task, NULL, &serail_out_task_attributes);
    osThreadNew(gpio_task, NULL, &gpio_task_attributes);
}
