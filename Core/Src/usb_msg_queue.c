#include "usb_msg_queue.h"
#include "cmsis_os2.h"
#include "common.h"
#include "main.h"

static osMessageQueueId_t g_cmd_queue_handle = NULL;

int usb_msg_queue_init(void)
{
    /* 16 * sizeof(cmd_packet) = 64 is a max usb packet */
    g_cmd_queue_handle = osMessageQueueNew(16 * 8, sizeof(cmd_packet), NULL);
    if (g_cmd_queue_handle == NULL) {
        log_err("osMessageQueueNew failed\n");
        return USB_MSG_FAILED;
    }

    return USB_MSG_OK;
}

int usb_msg_queue_deinit(void)
{
    osStatus_t ret;

    if (g_cmd_queue_handle == NULL) {
        log_err("cmd bridge need init\n");
        return USB_MSG_FAILED;
    }

    ret = osMessageQueueDelete(g_cmd_queue_handle);
    if (ret != osOK) {
        log_err("osMessageQueueDelete failed\n");
        return USB_MSG_FAILED;
    }
    g_cmd_queue_handle = NULL;

    return USB_MSG_OK;
}

int usb_msg_queue_put(const cmd_packet *packet)
{
    osStatus_t ret;

    if (packet == NULL) {
        log_err("packet is NULL\n");
        return USB_MSG_FAILED;
    }

    if (g_cmd_queue_handle == NULL) {
        log_err("cmd bridge need init\n");
        return USB_MSG_FAILED;
    }

    ret = osMessageQueuePut(g_cmd_queue_handle, packet, 0, 0);
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

    if (g_cmd_queue_handle == NULL) {
        log_err("cmd bridge need init\n");
        return USB_MSG_FAILED;
    }

    ret = osMessageQueueGet(g_cmd_queue_handle, packet, NULL, 0);
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
        log_err("Not support gpio pin: %d\n", pin_num);
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

typedef struct {
    intf_cmd_type cmd_type;
    intf_cmd_dir cmd_dir;
    int (*exec_func)(uint32_t addr, uint32_t offset, uint8_t *data, uint8_t len);
} cmd_exec_unit;

static const cmd_exec_unit g_cmd_exec_tab[] = {
    { INTF_CMD_TYPE_GPIO, INTF_CMD_DIR_IN, gpio_read_exec_func },
    { INTF_CMD_TYPE_GPIO, INTF_CMD_DIR_OUT, gpio_write_exec_func },
};

int msg_parse_exec(const cmd_packet *packet)
{
    int i, ret = USB_MSG_FAILED;

    for (i = 0; i < count_of(g_cmd_exec_tab); i++) {
        if (g_cmd_exec_tab[i].cmd_type == packet->cmd.bit.type && g_cmd_exec_tab[i].cmd_dir == packet->cmd.bit.dir) {
            ret = g_cmd_exec_tab[i].exec_func(packet->gpio.bit.group, packet->gpio.bit.pin,
                packet->data, packet->data_len);
            break;
        }
    }

    if (ret != USB_MSG_OK) {
        log_err("Cmd exec failed, type: %d, dir: %d, index: %d\n", packet->cmd.bit.type, packet->cmd.bit.dir, i);
        return USB_MSG_FAILED;
    }

    return USB_MSG_OK;
}