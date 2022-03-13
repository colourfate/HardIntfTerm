#ifndef _INTF_CMD_BRIDGE_
#define _INTF_CMD_BRIDGE_

#include "usbd_customhid.h"

#define INTF_PROTOCOL_PACKET_MAX CUSTOM_HID_EPOUT_SIZE
#define CHIP_PIN_MAX 16

enum {
    USB_MSG_OK = 0,
    USB_MSG_RETRY,
    USB_MSG_FAILED = -1
};

typedef enum {
    INTF_CMD_TYPE_GPIO = 0,
    INTF_CMD_TYPE_PWM,
    INTF_CMD_TYPE_ADC,
    INTF_CMD_TYPE_SERIAL,
    INTF_CMD_TYPE_I2C,
    INTF_CMD_TYPE_SPI,
    INTF_CMD_TYPE_CAN,
    INTF_CMD_TYPE_MAX
} intf_cmd_type;

typedef enum {
    INTF_CMD_DIR_IN = 0,
    INTF_CMD_DIR_OUT,
    INTF_CMD_DIR_MAX
} intf_cmd_dir;

typedef enum {
    CHIP_GPIOA = 0,
    CHIP_GPIOB,
    CHIP_GPIOC,
    CHIP_GPIOD,
    CHIP_GPIOE,
    CHIP_GPIOF,
    CHIP_GPIOG,
    CHIP_GPIOH,
    CHIP_MUL_FUNC,
    CHIP_GPIO_MAX
} chip_gpio_group;

typedef union {
    struct {
        uint8_t type : 7;
        uint8_t dir : 1;
    } bit;

    uint8_t data;
} intf_cmd;

typedef union {
    struct {
        uint8_t group : 4;
        uint8_t pin : 4;
    } bit;

    uint8_t data;
} intf_gpio;

typedef struct {
    intf_cmd cmd;
    intf_gpio gpio;
    uint8_t data_len;
    uint8_t data[1];
} cmd_packet;

int usb_msg_queue_init(void);
int usb_msg_queue_deinit(void);
int usb_msg_queue_put(const cmd_packet *packet);
int usb_msg_queue_get(cmd_packet *packet);

int msg_parse_exec(cmd_packet *packet);

#endif