#include "port_hal.h"
#include "common.h"
#include "usb_msg_queue.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "list.h"
#include "usart.h"
#include "gpio.h"
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

    if (attr == NULL || attr_len > ATTR_LEN_MAX) {
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
        
        // Find through pin number
        if (port_def->group == group && port_def->pin == pin && port_def->type == type && port_def->dir == dir) {
            return port_def;
        // Fnd through port number
        } else if (group == PORT_MUL_FUNC && port_def->type == type && 
            priv_cmp != NULL && priv_cmp(port_def->attr, pin)) {
            return port_def;
        }
    }

    return NULL;
}

static inline port_define *get_def_by_pin_num(port_group group, uint8_t pin, port_type type, port_dir dir)
{
    return get_port_define(group, pin, type, dir, NULL);
}

static inline port_define *get_def_by_port_num(port_type type, uint8_t port_num, 
    bool (*priv_cmp)(void *attr, uint8_t port_num))
{
    return get_port_define(PORT_MUL_FUNC, port_num, type, PORT_DIR_MAX, priv_cmp);
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

int port_hal_gpio_config(port_group group, uint8_t pin, port_type type, port_dir dir, void *attr)
{
    gpio_config *config = (gpio_config *)attr;
    GPIO_TypeDef *gpio_type = get_gpio_type_define(group);
    uint32_t gpio_pin = get_gpio_pin(pin);
    uint32_t gpio_dir = (dir == PORT_DIR_IN ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT_PP);

    if (attr == NULL) {
        log_err("Need port attribution\n");
        return USB_MSG_FAILED;
    }

    if (get_def_by_pin_num(group, pin, PORT_TYPE_GPIO, dir) != NULL) {
        log_err("The port have been register\n");
        return USB_MSG_FAILED;
    }

    config->speed = GPIO_SPEED_FREQ_LOW;
    gpio_init(gpio_type, gpio_pin, gpio_dir, config->speed);

    return USB_MSG_OK;
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

    port_def = get_def_by_pin_num(group, pin, PORT_TYPE_GPIO, PORT_DIR_IN);
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

    port_def = get_def_by_pin_num(group, pin, PORT_TYPE_GPIO, PORT_DIR_OUT);
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

static uint32_t g_baud_rate_tab[PORT_UART_BUAD_MAX] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400,
    460800, 921600, 1000000, 2000000, 4000000};
static uint32_t g_word_len_tab[PORT_UART_WORT_LEN_MAX] = {UART_WORDLENGTH_8B, UART_WORDLENGTH_9B};
static uint32_t g_stop_bit_tab[PORT_UART_STOP_BIT_MAX] = {UART_STOPBITS_1, UART_STOPBITS_2};
static uint32_t g_parity_tab[PORT_UART_PARITY_MAX] = {UART_PARITY_NONE, UART_PARITY_EVEN, UART_PARITY_ODD};
static uint32_t g_hwctl_tab[PORT_UART_HWCTL_MAX] = {UART_HWCONTROL_NONE, UART_HWCONTROL_RTS, UART_HWCONTROL_CTS,
    UART_HWCONTROL_RTS_CTS};

static inline bool uart_config_is_invalid(const uart_config *config)
{
    return config->buad_rate >= PORT_UART_BUAD_MAX || config->word_len >= PORT_UART_WORT_LEN_MAX ||
        config->stop_bit >= PORT_UART_STOP_BIT_MAX || config->parity >= PORT_UART_PARITY_MAX ||
        config->hwctl >= PORT_UART_HWCTL_MAX;
}

int port_hal_serial_config(const uart_config *config)
{
    int ret;
    port_define *port_def;
    UART_InitTypeDef hal_uart_init;
    
    if (config == NULL) {
        log_err("param is NULL\n");
        return osError;
    }

    if (uart_config_is_invalid(config)) {
        log_err("param is invalid\n");
        return osError;
    }
    
    port_def = get_def_by_port_num(PORT_TYPE_SERIAL, config->uart_num, serial_config_compare);
    if (port_def != NULL) {
        log_info("uart%d have been init\n", config->uart_num);
        return osOK;
    }

    hal_uart_init.BaudRate = g_baud_rate_tab[config->buad_rate];
    hal_uart_init.WordLength = g_word_len_tab[config->word_len];
    hal_uart_init.StopBits = g_stop_bit_tab[config->stop_bit];
    hal_uart_init.Parity = g_parity_tab[config->parity];
    hal_uart_init.Mode = UART_MODE_TX_RX;
    hal_uart_init.HwFlowCtl = g_hwctl_tab[config->hwctl];
    hal_uart_init.OverSampling = UART_OVERSAMPLING_16;
    ret = serial_init(config->uart_num, &hal_uart_init);
    if (ret != osOK) {
        log_err("serial_init failed\n");
        return osError;
    }

    return osOK;
}

int port_hal_serial_out(uint8_t uart_num, uint8_t *data, uint8_t len)
{
    port_define *port_def = get_def_by_port_num(PORT_TYPE_SERIAL, uart_num, serial_config_compare);
    uart_config *config;

    if (data == NULL) {
        log_err("invalid param\n");
        return osError;
    }

    if (port_def == NULL) {
        log_err("port not register: uart%d\n", uart_num);
        return osError;
    }

    config = (uart_config *)port_def->attr;
    return uart_transmit(config->uart_num, data, len);
}

int port_hal_serial_in(uint8_t uart_num, uint8_t *data, uint8_t *len)
{
    port_define *port_def = get_def_by_port_num(PORT_TYPE_SERIAL, uart_num, serial_config_compare);
    uart_config *config;

    if (data == NULL || len == NULL) {
        log_err("invalid param\n");
        goto exit;
    }

    if (port_def == NULL) {
        log_err("port not register: uart%d\n", uart_num);
        goto exit;
    }

    config = (uart_config *)port_def->attr;
    *len = read_uart_rx_buffer(config->uart_num, data, *len);

    return osOK;

exit:
    *len = 0;
    return osError;
}
