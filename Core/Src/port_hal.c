#include "port_hal.h"
#include "common.h"
#include "usb_msg_queue.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "list.h"
#include "usart.h"
#include <stdbool.h>

#define ATTR_LEN_MAX 64

static struct xLIST g_port_reg_tab;

typedef struct {
    struct xLIST_ITEM item;
    port_group group;
    uint8_t pin;
    port_type type;
    port_dir dir;
    uint8_t attr[0];
} port_define;

void port_hal_init(void)
{
    vListInitialise(&g_port_reg_tab);
}

static uint8_t get_attr_len(port_type type)
{
    switch (type) {
        case PORT_TYPE_GPIO:
            return 0;
        case PORT_TYPE_SERIAL:
            return sizeof(uart_config);
        default:
            return 0xff;
    }
}

int port_register(port_group group, uint8_t pin, port_type type, port_dir dir, void *attr)
{
    port_define *port_def;
    uint8_t attr_len = get_attr_len(type);
    uint8_t i;

    if (attr_len > ATTR_LEN_MAX) {
        log_err("invaild paramter\n");
        return osError;
    }

    if (!listLIST_IS_INITIALISED(&g_port_reg_tab)) {
        log_err("list not init\n");
        return osError;
    }

    for (i = 0; i < listCURRENT_LIST_LENGTH(&g_port_reg_tab); i++) {
        port_define *pd;

        listGET_OWNER_OF_NEXT_ENTRY(pd, &g_port_reg_tab);
        if (pd->group == group && pd->pin == pin) {
            uxListRemove(&pd->item);
            free(pd);
            break;
        }
    }

    port_def = malloc(sizeof(port_define) + attr_len);
    if (port_def == NULL) {
        log_err("malloc failed\n");
        return osError;
    }

    vListInitialiseItem(&port_def->item);
    listSET_LIST_ITEM_OWNER(&port_def->item, port_def);
    port_def->group = group;
    port_def->pin = pin;
    port_def->type = type;
    port_def->dir = dir;
    memcpy(port_def->attr, (uint8_t *)attr, attr_len);

    vListInsertEnd(&g_port_reg_tab, &port_def->item);

    return osOK;
}

int port_hal_deinit(void)
{
    uint8_t i;

    if (!listLIST_IS_INITIALISED(&g_port_reg_tab)) {
        log_err("list not init\n");
        return osError;
    }

    for (i = 0; i < listCURRENT_LIST_LENGTH(&g_port_reg_tab); i++) {
        port_define *port_def;

        listGET_OWNER_OF_NEXT_ENTRY(port_def, &g_port_reg_tab);
        uxListRemove(&port_def->item);
        free(port_def);
    }

    return osOK;
}

static port_define *get_port_define(port_group group, uint8_t pin, port_type type, port_dir dir,
    bool (*priv_cmp)(void *attr, uint8_t port_num))
{
    uint8_t i;

    if (!listLIST_IS_INITIALISED(&g_port_reg_tab)) {
        log_err("list not init\n");
        return NULL;
    }

    for (i = 0; i < listCURRENT_LIST_LENGTH(&g_port_reg_tab); i++) {
        port_define *port_def;

        listGET_OWNER_OF_NEXT_ENTRY(port_def, &g_port_reg_tab);
        
        if (port_def->group == group && port_def->pin == pin && port_def->type == type && port_def->dir == dir) {
            return port_def;
        } else if (group == PORT_MUL_FUNC && port_def->type == type && 
            priv_cmp != NULL && priv_cmp(port_def->attr, pin)) {
            return port_def;
        }
    }

    return NULL;
}

static GPIO_TypeDef *get_gpio_type_define(port_group gpio_group)
{
    GPIO_TypeDef *gpio_type;

    switch (gpio_group) {
        case PORT_GPIOA:
            gpio_type = GPIOA;
            break;
        case PORT_GPIOB:
            gpio_type = GPIOB;
            break;
        case PORT_GPIOC:
            gpio_type = GPIOC;
            break;
        case PORT_GPIOH:
            gpio_type = GPIOH;
            break;
        default:
            log_err("Not support gpio: %d\n", gpio_group);
            gpio_type = NULL;
    }

    return gpio_type;
}

static inline uint32_t get_gpio_pin(uint32_t pin_num)
{
    if (pin_num > 15) {
        log_err("Not support gpio pin: %lu\n", pin_num);
        return 0xffffffff;
    } 

    return (1 << pin_num);
}

int port_hal_gpio_read(port_group group, uint8_t pin, int *value)
{
    GPIO_TypeDef *gpio_type = get_gpio_type_define(group);
    uint32_t gpio_pin = get_gpio_pin(pin);
    port_define *port_def;

    if (value == NULL) {
        log_err("invalid param\n");
        return osError;
    }

    port_def = get_port_define(group, pin, PORT_TYPE_GPIO, PORT_DIR_IN, NULL);
    if (port_def == NULL) {
        log_err("port not register: %d %d\n", group, pin);
        return osError;
    }

    if (gpio_type == NULL || gpio_pin == 0xffffffff) {
        log_err("gpio invalid\n");
        return osError;
    }

    *value = HAL_GPIO_ReadPin(gpio_type, gpio_pin);
    
    return osOK;
}

int port_hal_gpio_write(port_group group, uint8_t pin, int value)
{
    GPIO_TypeDef *gpio_type = get_gpio_type_define(group);
    uint32_t gpio_pin = get_gpio_pin(pin);
    port_define *port_def;

    if (gpio_type == NULL || gpio_pin == 0xffffffff) {
        return osError;
    }

    port_def = get_port_define(group, pin, PORT_TYPE_GPIO, PORT_DIR_OUT, NULL);
    if (port_def == NULL) {
        log_err("port not register: %d %d\n", group, pin);
        return osError;
    }

    HAL_GPIO_WritePin(gpio_type, gpio_pin, value);

    return osOK;
}

static bool serial_config_compare(void *attr, uint8_t port_num)
{
    uart_config *config = (uart_config *)attr;
    return config->uart_num == port_num;
}

int port_hal_serial_out(port_group group, uint8_t pin, uint8_t *data, uint8_t len)
{
    port_define *port_def = get_port_define(group, pin, PORT_TYPE_SERIAL, PORT_DIR_OUT, serial_config_compare);
    uart_config *config;

    if (data == NULL) {
        log_err("invalid param\n");
        return osError;
    }

    if (group != PORT_MUL_FUNC) {
        log_err("invalid port group: %d\n", group);
        return osError;
    }

    if (port_def == NULL) {
        log_err("port not register: %d %d\n", group, pin);
        return osError;
    }

    config = (uart_config *)port_def->attr;
    return uart_transmit(config->uart_num - 1, data, len);
}

int port_hal_serial_in(port_group group, uint8_t pin, uint8_t *data, uint8_t *len)
{
    port_define *port_def = get_port_define(group, pin, PORT_TYPE_SERIAL, PORT_DIR_IN, serial_config_compare);
    uart_config *config;

    if (data == NULL || len == NULL) {
        log_err("invalid param\n");
        goto exit;
    }

    if (group != PORT_MUL_FUNC) {
        log_err("invalid port group: %d\n", group);
        goto exit;
    }

    if (port_def == NULL) {
        log_err("port not register: %d %d\n", group, pin);
        goto exit;
    }

    config = (uart_config *)port_def->attr;
    *len = read_uart_rx_buffer(config->uart_num - 1, data, *len);

    return osOK;

exit:
    *len = 0;
    return osError;
}