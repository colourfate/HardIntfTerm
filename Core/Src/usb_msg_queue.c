#include "usb_msg_queue.h"
#include "cmsis_os2.h"
#include "common.h"

static osMessageQueueId_t g_cmd_queue_handle = NULL;

int usb_msg_queue_init(void)
{
    // 16 * sizeof(cmd_packet) = 64 is a max usb packet
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
