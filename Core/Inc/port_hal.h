#ifndef _PORT_HAL_H_
#define _PORT_HAL_H_

#include "common.h"

typedef enum {
    PORT_TYPE_GPIO = 0,
    PORT_TYPE_PWM,
    PORT_TYPE_ADC,
    PORT_TYPE_SERIAL,
    PORT_TYPE_I2C,
    PORT_TYPE_SPI,
    PORT_TYPE_CAN,
    PORT_TYPE_MAX
} port_type;

typedef enum {
    PORT_DIR_IN = 0,
    PORT_DIR_OUT,
    PORT_DIR_MAX
} port_dir;

typedef enum {
    PORT_GPIOA = 0,
    PORT_GPIOB,
    PORT_GPIOC,
    PORT_GPIOD,
    PORT_GPIOE,
    PORT_GPIOF,
    PORT_GPIOG,
    PORT_GPIOH,
    PORT_MUL_FUNC,
    PORT_GPIO_MAX
} port_group;

typedef enum {
    PORT_UART_BUAD_1200 = 0,
    PORT_UART_BUAD_2400,
    PORT_UART_BUAD_4800,
    PORT_UART_BUAD_9600,
    PORT_UART_BUAD_19200,
    PORT_UART_BUAD_38400,
    PORT_UART_BUAD_57600,
    PORT_UART_BUAD_115200,
    PORT_UART_BUAD_230400,
    PORT_UART_BUAD_460800,
    PORT_UART_BUAD_921600,
    PORT_UART_BUAD_1M,
    PORT_UART_BUAD_2M,
    PORT_UART_BUAD_4M,
    PORT_UART_BUAD_MAX
} port_uart_buad_type;

typedef enum {
    PORT_UART_WORT_LEN_8 = 0,
    PORT_UART_WORT_LEN_9
} port_uart_word_len;

typedef enum {
    PORT_UART_STOP_BIT_1 = 0,
    PORT_UART_STOP_BIT_2
} port_uart_stop_bit;

typedef enum {
    PORT_UART_PARITY_NONE = 0,
    PORT_UART_PARITY_EVEN,
    PORT_UART_PARITY_ODD
} port_uart_parity;

typedef enum {
    PORT_UART_HWCTL_NONE = 0,
    PORT_UART_HWCTL_RTS,
    PORT_UART_HWCTL_CTS,
    PORT_UART_HWCTL_RTS_CTS
} port_uart_hwctl;

#define UART_NUM_MAX 8

typedef struct {
    uint8_t uart_num : 3;
    uint8_t buad_rate : 4;
    uint8_t word_len : 1;

    uint8_t stop_bit : 1;
    uint8_t parity : 2;
    uint8_t hwctl : 2;
} uart_config;

void port_hal_init(void);
int port_hal_deinit(void);
int port_hal_gpio_read(port_group group, uint8_t pin, int *value);
int port_hal_gpio_write(port_group group, uint8_t pin, int value);

#endif