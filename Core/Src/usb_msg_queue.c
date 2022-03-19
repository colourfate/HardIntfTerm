#include "usb_msg_queue.h"
#include "cmsis_os2.h"
#include "common.h"
#include "main.h"
#include "usart.h"

/* 
 * INIT --> CONFIG --> CMD <--> DATA --> EXIT
 *                      |---------------->|
 */
/*
typydef enum {
    EXEC_MACHINE_INIT,
    EXEC_MACHINE_CONFIG,
    EXEC_MACHINE_CMD,
    EXEC_MACHINE_DATA,
    EXEC_MACHINE_EXIT,
    EXEC_MACHINE_MAX
} exec_machine_state;
*/

typedef struct {
    //exec_machine_state state;
    osMessageQueueId_t cmd_queue;
} cmd_terminal;

static cmd_terminal g_termianl_panel;

int usb_msg_queue_init(void)
{
    g_termianl_panel.cmd_queue = osMessageQueueNew(8 * INTF_PROTOCOL_PACKET_MAX, 1, NULL);
    if (g_termianl_panel.cmd_queue == NULL) {
        log_err("osMessageQueueNew failed\n");
        return USB_MSG_FAILED;
    }

    return USB_MSG_OK;
}

int usb_msg_queue_deinit(void)
{
    if (g_termianl_panel.cmd_queue == NULL) {
        log_err("cmd bridge need init\n");
        return USB_MSG_FAILED;
    }

    (void)osMessageQueueDelete(g_termianl_panel.cmd_queue);
    g_termianl_panel.cmd_queue = NULL;

    return USB_MSG_OK;
}

int usb_msg_queue_put(const cmd_packet *packet)
{
    osStatus_t ret;
    uint8_t i;
    const uint8_t *p = (const uint8_t *)packet;

    if (packet == NULL) {
        log_err("packet is NULL\n");
        return USB_MSG_FAILED;
    }

    if (g_termianl_panel.cmd_queue == NULL) {
        log_err("cmd bridge need init\n");
        return USB_MSG_FAILED;
    }

    for (i = 0; i < sizeof(cmd_packet); i++) {
        ret = osMessageQueuePut(g_termianl_panel.cmd_queue, &p[i], 0, 0);
        if (ret != osOK) {
            log_err("put data failed\n");
            return USB_MSG_FAILED;
        }
    }

    p = &packet->data[0];
    for (i = 1; i < packet->data_len; i++) {
        ret = osMessageQueuePut(g_termianl_panel.cmd_queue, &p[i], 0, 0);
        if (ret != osOK){
            log_err("put msg failed\n");
            return USB_MSG_FAILED;
        }
    }

    return USB_MSG_OK;
}

int usb_msg_queue_block_get(cmd_packet *packet)
{
    osStatus_t ret;
    uint8_t i;
    uint8_t *p = (uint8_t *)packet;

    if (packet == NULL) {
        log_err("packet is NULL\n");
        return USB_MSG_FAILED;
    }

    if (g_termianl_panel.cmd_queue == NULL) {
        log_err("cmd bridge need init\n");
        return USB_MSG_FAILED;
    }

    for (i = 0; i < sizeof(cmd_packet); i++) {
        ret = osMessageQueueGet(g_termianl_panel.cmd_queue, &p[i], NULL, osWaitForever);
        if (ret != osOK){
            log_err("get msg failed\n");
            return USB_MSG_FAILED;
        }
    }
    
    p = &packet->data[0];
    for (i = 1; i < packet->data_len; i++) {
        ret = osMessageQueueGet(g_termianl_panel.cmd_queue, &p[i], NULL, osWaitForever);
        if (ret != osOK){
            log_err("get msg failed\n");
            return USB_MSG_FAILED;
        }
    }

    return USB_MSG_OK;
}

/* For stm32f4x1 */
static GPIO_TypeDef *get_gpio_type_define(chip_gpio_group gpio_group)
{
    GPIO_TypeDef *gpio_type;

    switch (gpio_group) {
        case CHIP_GPIOA:
            gpio_type = GPIOA;
            break;
        case CHIP_GPIOB:
            gpio_type = GPIOB;
            break;
        case CHIP_GPIOC:
            gpio_type = GPIOC;
            break;
        case CHIP_GPIOH:
            gpio_type = GPIOH;
            break;
        default:
            log_err("Not support gpio: %d\n", gpio_group);
            gpio_type = NULL;
    }

    return gpio_type;
}

/* For stm32f4x1 */
static inline uint32_t get_gpio_pin(uint32_t pin_num)
{
    if (pin_num > 15) {
        log_err("Not support gpio pin: %lu\n", pin_num);
        return 0xffffffff;
    } 

    return (1 << pin_num);
}

static int gpio_read_exec_func(cmd_packet *packet)
{
    GPIO_TypeDef *gpio_type = get_gpio_type_define(packet->gpio.bit.group);
    uint32_t gpio_pin = get_gpio_pin(packet->gpio.bit.pin);

    if (gpio_type == NULL || gpio_pin == 0xffffffff) {
        return USB_MSG_FAILED;
    }

    if (packet->data_len != 1) {
        log_err("Invalid param len: %d\n", packet->data_len);
        return USB_MSG_FAILED;
    }

    packet->data[0] = HAL_GPIO_ReadPin(gpio_type, gpio_pin);

    return USB_MSG_OK;
}

static int gpio_write_exec_func(cmd_packet *packet)
{
    GPIO_TypeDef *gpio_type = get_gpio_type_define(packet->gpio.bit.group);
    uint32_t gpio_pin = get_gpio_pin(packet->gpio.bit.pin);

    if (gpio_type == NULL || gpio_pin == 0xffffffff) {
        return USB_MSG_FAILED;
    }

    if (packet->data[0] != 0 && packet->data[0] != 1) {
        log_err("Invalid param data: %d\n", packet->data[0]);
        return USB_MSG_FAILED;
    }

    HAL_GPIO_WritePin(gpio_type, gpio_pin, packet->data[0]);

    return USB_MSG_OK;
}

static int serial_in_exec_func(cmd_packet *packet)
{
    if (packet->gpio.bit.pin > 2) {
        log_err("Not serial num: %d\n", packet->gpio.bit.pin);
        return USB_MSG_FAILED;
    }

    packet->data_len = read_uart2_rx_buffer(&packet->data[0], packet->data_len);

    return USB_MSG_OK;
}

static int serial_out_exec_func(cmd_packet *packet)
{
    if (packet->gpio.bit.pin > 2) {
        log_err("Not serial num: %d\n", packet->gpio.bit.pin);
        return USB_MSG_FAILED;
    }

    /* Only uart2 in the chip */
    HAL_UART_Transmit(&huart2, &packet->data[0], packet->data_len, 0xFFFF);

    return USB_MSG_OK;
}

typedef struct {
    intf_cmd_type cmd_type;
    intf_cmd_dir cmd_dir;
    int (*entry)(cmd_packet *packet);
} cmd_exec_unit;

static const cmd_exec_unit g_cmd_exec_tab[] = {
    { INTF_CMD_TYPE_GPIO, INTF_CMD_DIR_IN, gpio_read_exec_func },
    { INTF_CMD_TYPE_GPIO, INTF_CMD_DIR_OUT, gpio_write_exec_func },
    { INTF_CMD_TYPE_SERIAL, INTF_CMD_DIR_IN, serial_in_exec_func },
    { INTF_CMD_TYPE_SERIAL, INTF_CMD_DIR_OUT, serial_out_exec_func },
};

int msg_parse_exec(cmd_packet *packet)
{
    int ret = USB_MSG_FAILED;
    uint8_t i;

    if (packet == NULL) {
        log_err("packet is NULL\n");
        return USB_MSG_FAILED;
    }

    if (packet->data_len > INTF_PROTOCOL_PACKET_MAX - sizeof(cmd_packet) - 1) {
        log_err("packet is too large: %d\n", packet->data_len);
        return USB_MSG_FAILED;
    }

    for (i = 0; i < count_of(g_cmd_exec_tab); i++) {
        if (g_cmd_exec_tab[i].cmd_type == packet->cmd.bit.type && g_cmd_exec_tab[i].cmd_dir == packet->cmd.bit.dir) {
            ret = g_cmd_exec_tab[i].entry(packet);
            break;
        }
    }

    if (ret != USB_MSG_OK) {
        log_err("Cmd exec failed, type: %d, dir: %d\n", packet->cmd.bit.type, packet->cmd.bit.dir);
        return USB_MSG_FAILED;
    }

    return USB_MSG_OK;
}