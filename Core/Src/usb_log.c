#include <string.h>
#include "usb_log.h"
#include "usbd_cdc_if.h"
#include "usb_msg_queue.h"

#define MAX_INFO_LEN 128

/* Overwrite printf */
int _write(int file, char *ptr, int len)
{
    uint8_t raw_data[MAX_INFO_LEN] = {0};
    cmd_packet *packet;
    int ret;

    if (len > MAX_INFO_LEN - sizeof(cmd_packet)) {
        len = MAX_INFO_LEN - sizeof(cmd_packet);
    }

    packet = (cmd_packet *)raw_data;
    packet->cmd.bit.mode = INTF_CMD_MODE_INFO;
    packet->gpio.data = 0;
    packet->data_len = len;
    memcpy(packet->data, ptr, len);
    if (packet->data[len - 1] == '\n') {
        packet->data[len] = '\r';
        len++;
    }

    do {
        ret = CDC_Transmit_FS(raw_data, len + sizeof(cmd_packet) - 1);
    } while (ret == USBD_BUSY);

    return len;
}
