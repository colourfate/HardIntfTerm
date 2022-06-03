#ifndef _INTF_CMD_BRIDGE_
#define _INTF_CMD_BRIDGE_

#include "usbd_cdc.h"

#define INTF_PROTOCOL_PACKET_MAX CDC_DATA_FS_MAX_PACKET_SIZE
#define CHIP_PIN_MAX 16

typedef enum {
    INTF_CMD_MODE_CTRL = 0,
    INTF_CMD_MODE_CFG,
    INTF_CMD_MODE_INFO,
    INTF_CMD_MODE_MAX
} intf_cmd_mode;

typedef union {
    struct {
        uint8_t dir : 1;
        uint8_t mode : 2;
        uint8_t type : 5;
    } bit;

    uint8_t data;
} intf_cmd;

typedef union {
    struct {
        uint8_t pin : 4;
        uint8_t group : 4;
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
int usb_msg_queue_rawput(uint8_t *data, uint8_t len);
int usb_msg_queue_put(const cmd_packet *packet);
int usb_msg_queue_block_get(cmd_packet *packet);

int msg_parse_exec(cmd_packet *packet);

#endif