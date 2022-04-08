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

const osThreadAttr_t packet2_task_attributes = {
  .name = "packet2_task",
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

/*
 * Description: Send 2 packet at a time, gpio read and uart read respectively
 * Expect: read GPIO and read uart2
 */
void pack2_task(void *argument)
{
    /*
    cmd_packet *uart_packet;
    cmd_packet gpio_packet;
    const uint8_t len = 10;

    uart_packet = malloc(sizeof(cmd_packet) + len - 1);
    uart_packet->cmd.bit.type = INTF_CMD_TYPE_SERIAL;
    uart_packet->cmd.bit.dir = INTF_CMD_DIR_IN;
    uart_packet->gpio.bit.group = CHIP_MUL_FUNC;
    uart_packet->gpio.bit.pin = 2;
    uart_packet->data_len = len;

    gpio_packet.cmd.bit.type = INTF_CMD_TYPE_GPIO;
    gpio_packet.cmd.bit.dir = INTF_CMD_DIR_IN;
    gpio_packet.gpio.bit.group = CHIP_GPIOA;
    gpio_packet.gpio.bit.pin = 0;
    gpio_packet.data_len = 1;
    gpio_packet.data[0] = 0;

    for (;;) {
        usb_msg_queue_put(uart_packet);
        usb_msg_queue_put(&gpio_packet);
        osDelay(1000);
    }
    */
    int8_t test_buf[] = {0, 0, 1, 0, 6, -126, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    for (;;) {
        usb_msg_queue_rawput(test_buf, 17);
        osDelay(1000);
    }
}

void test_case_init(void)
{
    //osThreadNew(serail_in_task, NULL, &serail_in_task_attributes);
    //osThreadNew(serail_out_task, NULL, &serail_out_task_attributes);
    //osThreadNew(gpio_task, NULL, &gpio_task_attributes);
    osThreadNew(pack2_task, NULL, &packet2_task_attributes);
}
