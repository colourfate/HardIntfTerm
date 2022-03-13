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

typedef int (*exec_func)(uint32_t addr, uint32_t offset, uint8_t *data, uint8_t len);

typedef struct {
    //exec_machine_state state;
    exec_func last_entry;
    cmd_packet last_packet;
    osMessageQueueId_t cmd_queue;
} cmd_terminal;

static cmd_terminal g_termianl_panel = {
    .last_packet.data_len = 1
};

int usb_msg_queue_init(void)
{
    /* 16 * sizeof(cmd_packet) = 64 is a max usb packet */
    g_termianl_panel.cmd_queue = osMessageQueueNew(16 * 8, sizeof(cmd_packet), NULL);
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

    if (packet == NULL) {
        log_err("packet is NULL\n");
        return USB_MSG_FAILED;
    }

    if (g_termianl_panel.cmd_queue == NULL) {
        log_err("cmd bridge need init\n");
        return USB_MSG_FAILED;
    }

    ret = osMessageQueuePut(g_termianl_panel.cmd_queue, packet, 0, 0);
    if (ret != osOK) {
        log_err("queue data failed\n");
        return USB_MSG_FAILED;
    }

    return USB_MSG_OK;
}

int usb_msg_queue_get(cmd_packet *packet)
{
    osStatus_t ret;

    if (packet == NULL) {
        log_err("packet is NULL\n");
        return USB_MSG_FAILED;
    }

    if (g_termianl_panel.cmd_queue == NULL) {
        log_err("cmd bridge need init\n");
        return USB_MSG_FAILED;
    }

    ret = osMessageQueueGet(g_termianl_panel.cmd_queue, packet, NULL, 0);
    if (ret == osErrorResource) {
        return USB_MSG_RETRY;
    } else if (ret != osOK){
        return USB_MSG_FAILED;
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

static int gpio_read_exec_func(uint32_t gpio_group, uint32_t pin_num, uint8_t *data, uint8_t len)
{
    GPIO_TypeDef *gpio_type = get_gpio_type_define(gpio_group);
    uint32_t gpio_pin = get_gpio_pin(pin_num);

    if (gpio_type == NULL || gpio_pin == 0xffffffff) {
        return USB_MSG_FAILED;
    }

    if (data == NULL || len != 1) {
        log_err("Invalid param data: %p, len: %d\n", data, len);
        return USB_MSG_FAILED;
    }

    *data = HAL_GPIO_ReadPin(gpio_type, gpio_pin);

    return USB_MSG_OK;
}

static int gpio_write_exec_func(uint32_t gpio_group, uint32_t pin_num, uint8_t *data, uint8_t len)
{
    GPIO_TypeDef *gpio_type = get_gpio_type_define(gpio_group);
    uint32_t gpio_pin = get_gpio_pin(pin_num);

    UNUSED(len);

    if (gpio_type == NULL || gpio_pin == 0xffffffff) {
        return USB_MSG_FAILED;
    }

    if (data == NULL || (*data != 0 && *data != 1)) {
        log_err("Invalid param data: %p\n", data);
        return USB_MSG_FAILED;
    }

    HAL_GPIO_WritePin(gpio_type, gpio_pin, *data);

    return USB_MSG_OK;
}

static int serial_out_exec_func(uint32_t gpio_group, uint32_t serial_num, uint8_t *data, uint8_t len)
{
    UNUSED(gpio_group);

    if (serial_num > 2) {
        log_err("Not serial num: %d\n", serial_num);
        return USB_MSG_FAILED;
    }

    if (data == NULL) {
        log_err("Invalid param data: %p\n", data);
        return USB_MSG_FAILED;
    }

    /* Only uart2 in the chip */
    //log_err("%c: %d\n", *data, len);
    HAL_UART_Transmit(&huart2, data, len, 0xFFFF);

    return USB_MSG_OK;
}

typedef struct {
    intf_cmd_type cmd_type;
    intf_cmd_dir cmd_dir;
    exec_func entry;
} cmd_exec_unit;

static const cmd_exec_unit g_cmd_exec_tab[] = {
    { INTF_CMD_TYPE_GPIO, INTF_CMD_DIR_IN, gpio_read_exec_func },
    { INTF_CMD_TYPE_GPIO, INTF_CMD_DIR_OUT, gpio_write_exec_func },
    { INTF_CMD_TYPE_SERIAL, INTF_CMD_DIR_OUT, serial_out_exec_func },
};

int msg_parse_exec(cmd_packet *packet)
{
    int ret = USB_MSG_FAILED;
    exec_func cur_entry = NULL;
    cmd_packet *cur_packet = NULL;
    uint8_t cur_len, i;
    uint8_t *cur_data = NULL;

    if (packet == NULL) {
        log_err("packet is NULL\n");
        return USB_MSG_FAILED;
    }

    if (g_termianl_panel.last_packet.data_len == 1 || g_termianl_panel.last_packet.data_len == 0) {
        for (i = 0; i < count_of(g_cmd_exec_tab); i++) {
            if (g_cmd_exec_tab[i].cmd_type == packet->cmd.bit.type &&
                g_cmd_exec_tab[i].cmd_dir == packet->cmd.bit.dir) {
                cur_entry = g_cmd_exec_tab[i].entry;
                cur_packet = packet;
                cur_len = 1;
                cur_data = packet->data;

                g_termianl_panel.last_entry = g_cmd_exec_tab[i].entry;
                g_termianl_panel.last_packet = *packet;
                break;
            }
        }

        if (i == count_of(g_cmd_exec_tab)) {
            log_err("Not find entry, type: %d, dir: %d\n", packet->cmd.bit.type, packet->cmd.bit.dir);
            return USB_MSG_FAILED;
        }
    } else {
        cur_entry = g_termianl_panel.last_entry;
        cur_packet = &g_termianl_panel.last_packet;
        if (g_termianl_panel.last_packet.data_len >= sizeof(cmd_packet)) {
            cur_len = sizeof(cmd_packet);
            g_termianl_panel.last_packet.data_len -= sizeof(cmd_packet);
        } else {
            cur_len = g_termianl_panel.last_packet.data_len;
            g_termianl_panel.last_packet.data_len = 0;
        }
        cur_data = (uint8_t *)packet;
    }

    ret = cur_entry(cur_packet->gpio.bit.group, cur_packet->gpio.bit.pin, cur_data, cur_len);
    if (ret != USB_MSG_OK) {
        log_err("Cmd exec failed, type: %d, dir: %d\n", packet->cmd.bit.type, packet->cmd.bit.dir);
        return USB_MSG_FAILED;
    }

    return USB_MSG_OK;
}