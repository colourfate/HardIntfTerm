#include "usb_msg_queue.h"
#include "cmsis_os2.h"
#include "usbd_cdc_if.h"
#include "usb_log.h"
#include "main.h"
#include "usart.h"
#include "port_hal.h"
#include "common.h"

typedef struct {
    osMessageQueueId_t cmd_queue;
} cmd_terminal;

static cmd_terminal g_terminal_panel;

int usb_msg_queue_init(void)
{
    g_terminal_panel.cmd_queue = osMessageQueueNew(8 * INTF_PROTOCOL_PACKET_MAX, 1, NULL);
    if (g_terminal_panel.cmd_queue == NULL) {
        log_err("osMessageQueueNew failed\n");
        return USB_MSG_FAILED;
    }

    port_hal_init();

    return USB_MSG_OK;
}

int usb_msg_queue_deinit(void)
{
    if (g_terminal_panel.cmd_queue == NULL) {
        log_err("cmd bridge need init\n");
        return USB_MSG_FAILED;
    }

    (void)osMessageQueueDelete(g_terminal_panel.cmd_queue);
    g_terminal_panel.cmd_queue = NULL;

    port_hal_deinit();

    return USB_MSG_OK;
}

/* Called at usb interrupt, don't put log_err here  */
int usb_msg_queue_rawput(uint8_t *data, uint8_t len)
{
    uint8_t i;
    uint32_t ret;

    if (data == NULL) {
        return USB_MSG_FAILED;
    }

    if (len > INTF_PROTOCOL_PACKET_MAX) {
        return USB_MSG_FAILED;
    }

    for (i = 0; i < len; i++) {
        ret = osMessageQueuePut(g_terminal_panel.cmd_queue, &data[i], 0, 0);
        if (ret != osOK) {
            return USB_MSG_FAILED;
        }
    }

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

    if (g_terminal_panel.cmd_queue == NULL) {
        log_err("cmd bridge need init\n");
        return USB_MSG_FAILED;
    }

    for (i = 0; i < sizeof(cmd_packet); i++) {
        ret = osMessageQueuePut(g_terminal_panel.cmd_queue, &p[i], 0, 0);
        if (ret != osOK) {
            log_err("put data failed\n");
            return USB_MSG_FAILED;
        }
    }

    p = &packet->data[0];
    for (i = 1; i < packet->data_len; i++) {
        ret = osMessageQueuePut(g_terminal_panel.cmd_queue, &p[i], 0, 0);
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

    if (g_terminal_panel.cmd_queue == NULL) {
        log_err("cmd bridge need init\n");
        return USB_MSG_FAILED;
    }

    for (i = 0; i < sizeof(cmd_packet); i++) {
        ret = osMessageQueueGet(g_terminal_panel.cmd_queue, &p[i], NULL, osWaitForever);
        if (ret != osOK){
            log_err("get msg failed\n");
            return USB_MSG_FAILED;
        }
    }

    p = &packet->data[0];
    for (i = 1; i < packet->data_len; i++) {
        ret = osMessageQueueGet(g_terminal_panel.cmd_queue, &p[i], NULL, osWaitForever);
        if (ret != osOK){
            log_err("get msg failed\n");
            return USB_MSG_FAILED;
        }
    }

    return USB_MSG_OK;
}

static int gpio_read_exec_func(cmd_packet *packet)
{
    int ret, value;

    log_info("\n");
    if (packet->data_len != 1) {
        log_err("Invalid param len: %d\n", packet->data_len);
        return USB_MSG_FAILED;
    }

    ret = port_hal_gpio_read(packet->gpio.bit.group, packet->gpio.bit.pin, &value);
    if (ret != osOK) {
        log_err("port_hal_gpio_read failed\n");
        return USB_MSG_FAILED;
    }

    packet->data[0] = (uint8_t)value;

    return USB_MSG_OK;
}

static int gpio_write_exec_func(cmd_packet *packet)
{
    int ret;

    log_info("\n");
    if (packet->data[0] != 0 && packet->data[0] != 1) {
        log_err("Invalid param data: %d\n", packet->data[0]);
        return USB_MSG_FAILED;
    }

    ret = port_hal_gpio_write(packet->gpio.bit.group, packet->gpio.bit.pin, packet->data[0]);
    if (ret != osOK) {
        log_err("port_hal_gpio_read failed\n");
        return USB_MSG_FAILED;
    }

    return USB_MSG_OK;
}

static int serial_in_exec_func(cmd_packet *packet)
{
    log_info("\n");
    if (packet->gpio.bit.group != PORT_MUL_FUNC) {
        log_err("serial group type invalid: %d\n", packet->gpio.bit.group);
        return 0;
    }

    return port_hal_serial_in(packet->gpio.bit.pin, packet->data, &packet->data_len);;
}

static int serial_out_exec_func(cmd_packet *packet)
{
    log_info("\n");
    if (packet->gpio.bit.group != PORT_MUL_FUNC) {
        log_err("serial group type invalid: %d\n", packet->gpio.bit.group);
        return USB_MSG_FAILED;
    }

    return port_hal_serial_out(packet->gpio.bit.pin, packet->data, packet->data_len);
}

static int pwm_write_exec_func(cmd_packet *packet)
{
    int ret;
    uint16_t value;
    log_info("\n");

    if (packet->data_len != 2) {
        log_err("Invalid param len: %d\n", packet->data_len);
        return USB_MSG_FAILED;
    }

    value = packet->data[0] | packet->data[1] << 8;
    if (value > HAL_PWM_MAX_PULSE) {
        log_err("Invalid param data: %d\n", packet->data[0]);
        return USB_MSG_FAILED;
    }

    ret = port_hal_pwm_write(packet->gpio.bit.group, packet->gpio.bit.pin, value);
    if (ret != osOK) {
        log_err("port_hal_gpio_read failed\n");
        return USB_MSG_FAILED;
    }

    return USB_MSG_OK;
}

static int gpio_cfg_exec_func(cmd_packet *packet)
{
    int ret;

    log_info("\n");
    ret = port_hal_gpio_config(packet->gpio.bit.group, packet->gpio.bit.pin, packet->cmd.bit.dir, (void *)packet->data);
    if (ret != osOK) {
        log_err("port config failed\n");
        return ret;
    }

    return port_register(packet->gpio.bit.group, packet->gpio.bit.pin,
        packet->cmd.bit.type, packet->cmd.bit.dir, packet->data, packet->data_len);
}

static int serial_cfg_exec_func(cmd_packet *packet)
{
    int ret;
    log_info("\n");

    if (packet->data_len != sizeof(uart_config)) {
        log_err("invalid data len: %d\n", packet->data_len);
        return osError;
    }

    ret = port_hal_serial_config((void *)packet->data);
    if (ret != osOK) {
        log_err("port config failed\n");
        return ret;
    }

    return port_register(packet->gpio.bit.group, packet->gpio.bit.pin,
        packet->cmd.bit.type, packet->cmd.bit.dir, packet->data, packet->data_len);
}

static int pwm_cfg_exec_func(cmd_packet *packet)
{
    int ret;
    log_info("\n");

    if (packet->data_len != sizeof(pwm_config)) {
        log_err("invalid data len: %d\n", packet->data_len);
        return osError;
    }

    ret = port_hal_pwm_config(packet->gpio.bit.group, packet->gpio.bit.pin, (void *)packet->data);
    if (ret != osOK) {
        log_err("port config failed\n");
        return ret;
    }

    return port_register(packet->gpio.bit.group, packet->gpio.bit.pin,
        packet->cmd.bit.type, packet->cmd.bit.dir, packet->data, packet->data_len);
}

typedef struct {
    port_type cmd_type;
    intf_cmd_mode cmd_mode;
    port_dir cmd_dir;
    int (*entry)(cmd_packet *packet);
} cmd_exec_unit;

static const cmd_exec_unit g_cmd_exec_tab[] = {
    /* control function */
    { PORT_TYPE_GPIO, INTF_CMD_MODE_CTRL, PORT_DIR_IN, gpio_read_exec_func },
    { PORT_TYPE_GPIO, INTF_CMD_MODE_CTRL, PORT_DIR_OUT, gpio_write_exec_func },
    { PORT_TYPE_SERIAL, INTF_CMD_MODE_CTRL, PORT_DIR_IN, serial_in_exec_func },
    { PORT_TYPE_SERIAL, INTF_CMD_MODE_CTRL, PORT_DIR_OUT, serial_out_exec_func },
    { PORT_TYPE_PWM, INTF_CMD_MODE_CTRL, PORT_DIR_OUT, pwm_write_exec_func },
    /* config function */
    { PORT_TYPE_GPIO, INTF_CMD_MODE_CFG, PORT_DIR_MAX, gpio_cfg_exec_func },
    { PORT_TYPE_SERIAL, INTF_CMD_MODE_CFG, PORT_DIR_MAX, serial_cfg_exec_func },
    { PORT_TYPE_PWM, INTF_CMD_MODE_CFG, PORT_DIR_OUT, pwm_cfg_exec_func }
};

int msg_parse_exec(cmd_packet *packet)
{
    int ret = USB_MSG_FAILED;
    uint8_t i;

    if (packet == NULL) {
        log_err("packet is NULL\n");
        return USB_MSG_FAILED;
    }

    if (packet->data_len > INTF_PROTOCOL_PACKET_MAX - sizeof(cmd_packet) + 1) {
        log_err("packet is too large: %d\n", packet->data_len);
        return USB_MSG_FAILED;
    }

    for (i = 0; i < count_of(g_cmd_exec_tab); i++) {
        if (g_cmd_exec_tab[i].cmd_type == packet->cmd.bit.type && g_cmd_exec_tab[i].cmd_mode == packet->cmd.bit.mode &&
            (g_cmd_exec_tab[i].cmd_dir == PORT_DIR_MAX || g_cmd_exec_tab[i].cmd_dir == packet->cmd.bit.dir)) {
            ret = g_cmd_exec_tab[i].entry(packet);
            break;
        }
    }

    if (ret != USB_MSG_OK) {
        log_err("Cmd exec failed, type: %d, mode: %d, dir: %d\n", packet->cmd.bit.type, packet->cmd.bit.mode,
            packet->cmd.bit.dir);
        return USB_MSG_FAILED;
    }

    return USB_MSG_OK;
}
