#include "test_case.h"
#include "FreeRTOS.h"
#include <string.h>
#include "usb_msg_queue.h"
#include "usb_log.h"
#include "cmsis_os2.h"
#include "port_hal.h"

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
    cmd_packet packet;
    packet.cmd.bit.type = PORT_TYPE_GPIO;
    packet.cmd.bit.dir = PORT_DIR_IN;
    packet.cmd.bit.mode = INTF_CMD_MODE_CFG;
    packet.gpio.bit.group = PORT_GPIOA;
    packet.gpio.bit.pin = 0;
    packet.data_len = 1;
    usb_msg_queue_put(&packet);

    packet.cmd.bit.type = PORT_TYPE_GPIO;
    packet.cmd.bit.dir = PORT_DIR_OUT;
    packet.cmd.bit.mode = INTF_CMD_MODE_CFG;
    packet.gpio.bit.group = PORT_GPIOC;
    packet.gpio.bit.pin = 13;
    packet.data_len = 1;
    usb_msg_queue_put(&packet);

    for(;;) {
        osDelay(1000);
        packet.cmd.bit.type = PORT_TYPE_GPIO;
        packet.cmd.bit.dir = PORT_DIR_OUT;
        packet.cmd.bit.mode = INTF_CMD_MODE_CTRL;
        packet.gpio.bit.group = PORT_GPIOC;
        packet.gpio.bit.pin = 13;
        packet.data_len = 1;
        packet.data[0] = 0;
        usb_msg_queue_put(&packet);

        osDelay(1000);
        packet.data[0] = 1;
        usb_msg_queue_put(&packet);

        packet.cmd.bit.type = PORT_TYPE_GPIO;
        packet.cmd.bit.dir = PORT_DIR_IN;
        packet.cmd.bit.mode = INTF_CMD_MODE_CTRL;
        packet.gpio.bit.group = PORT_GPIOA;
        packet.gpio.bit.pin = 0;
        packet.data_len = 1;
        packet.data[0] = 0;
        usb_msg_queue_put(&packet);
    }
}

static void uart_attr_set(uint32_t uart_num)
{
    cmd_packet *cfg_packet;
    uart_config *uart_attr;

    cfg_packet = malloc(sizeof(cmd_packet) + sizeof(uart_config) - 1);
    cfg_packet->cmd.bit.type = PORT_TYPE_SERIAL;
    cfg_packet->cmd.bit.mode = INTF_CMD_MODE_CFG;
    cfg_packet->data_len = sizeof(uart_config);
    uart_attr = (uart_config *)(cfg_packet->data);
    uart_attr->uart_num = uart_num;
    uart_attr->buad_rate = PORT_UART_BUAD_115200;
    uart_attr->word_len = PORT_UART_WORT_LEN_8;
    uart_attr->stop_bit = PORT_UART_STOP_BIT_1;
    uart_attr->parity = PORT_UART_PARITY_NONE;
    uart_attr->hwctl = PORT_UART_HWCTL_NONE;

    if (uart_num == 2) {
        cfg_packet->gpio.bit.group = PORT_GPIOA;
        cfg_packet->gpio.bit.pin = 2;
        cfg_packet->cmd.bit.dir = PORT_DIR_OUT;
        usb_msg_queue_put(cfg_packet);

        cfg_packet->gpio.bit.group = PORT_GPIOA;
        cfg_packet->gpio.bit.pin = 3;
        cfg_packet->cmd.bit.dir = PORT_DIR_IN;
        usb_msg_queue_put(cfg_packet);
    } else if (uart_num == 1) {
        cfg_packet->gpio.bit.group = PORT_GPIOA;
        cfg_packet->gpio.bit.pin = 15;
        cfg_packet->cmd.bit.dir = PORT_DIR_OUT;
        usb_msg_queue_put(cfg_packet);

        cfg_packet->gpio.bit.group = PORT_GPIOB;
        cfg_packet->gpio.bit.pin = 3;
        cfg_packet->cmd.bit.dir = PORT_DIR_IN;
        usb_msg_queue_put(cfg_packet);
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
    uint32_t uart_num = (uint32_t)(uintptr_t)argument;

    uart_attr_set(uart_num);
    packet = malloc(sizeof(cmd_packet) + strlen(str) - 1);
    packet->cmd.bit.type = PORT_TYPE_SERIAL;
    packet->cmd.bit.dir = PORT_DIR_OUT;
    packet->cmd.bit.mode = INTF_CMD_MODE_CTRL;
    packet->gpio.bit.group = PORT_MUL_FUNC;
    packet->gpio.bit.pin = uart_num;
    packet->data_len = strlen(str);
    strcpy((char *)packet->data, str);
    osDelay(1000);

    for (;;) {
        usb_msg_queue_put(packet);
        osDelay(1000);
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
    uint32_t uart_num = (uint32_t)(uintptr_t)argument;
    uart_attr_set(uart_num);

    packet = malloc(sizeof(cmd_packet) + len - 1);
    packet->cmd.bit.type = PORT_TYPE_SERIAL;
    packet->cmd.bit.dir = PORT_DIR_IN;
    packet->cmd.bit.mode = INTF_CMD_MODE_CTRL;
    packet->gpio.bit.group = PORT_MUL_FUNC;
    packet->gpio.bit.pin = uart_num;
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
    osThreadNew(serail_in_task, (void *)(uintptr_t)1, &serail_in_task_attributes);
    osThreadNew(serail_out_task, (void *)(uintptr_t)1, &serail_out_task_attributes);
    osThreadNew(gpio_task, NULL, &gpio_task_attributes);
}
