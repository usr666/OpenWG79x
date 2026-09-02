#include "remotecom.h"
#include "hal/hal_remotecom.h"
#include "hal/hal_sensors.h"
#include "hal/hal_motor.h"
#include "hal/hal_charger.h"
#include "mowercontrol.h"
#include "system.h"

#define RC_START 0x7E
#define RC_ESC 0x7D
#define RC_ESC_XOR 0x20

#define MSG_MOWER_STATUS 0x00
#define MSG_REMOTE_CONTROL_RUN 0x01
#define MSG_REMOTE_CONTROL_TURN 0x02

#define MOWER_STATUS_LEN 6
#define REMOTE_CONTROL_RUN_LEN 4
#define REMOTE_CONTROL_TURN_LEN 5
#define REMOTECOM_MAX_PAYLOAD 8 // largest payload currently defined, with some headroom

#define STATUS_SEND_INTERVAL_MS 1000

#define TX_BUF_SIZE 64 // must be a power of two
#define TX_BUF_MASK (TX_BUF_SIZE - 1)
#define TX_BYTES_PER_TASK 10

typedef enum {
    rx_wait_start = 0,
    rx_msg_id,
    rx_len,
    rx_payload,
    rx_crc_hi,
    rx_crc_lo
} rx_state_t;

static rx_state_t rx_state;
static bool rx_escape_pending;
static uint8_t rx_msg_id_byte;
static uint8_t rx_len_byte;
static uint8_t rx_payload_buf[REMOTECOM_MAX_PAYLOAD];
static uint8_t rx_payload_idx;
static uint16_t rx_running_crc;
static uint16_t rx_received_crc;

static uint8_t tx_buf[TX_BUF_SIZE];
static uint8_t tx_head;
static uint8_t tx_tail;

static systimer_t status_send_timer;

static uint16_t crc16_update(uint16_t crc, uint8_t data)
{
    uint8_t i;

    crc ^= (uint16_t)data << 8;
    for (i = 0; i < 8; i++) {
        crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

static uint8_t tx_free(void)
{
    return (uint8_t)((tx_tail - tx_head - 1) & TX_BUF_MASK);
}

static void uart_put_raw(uint8_t b)
{
    tx_buf[tx_head] = b;
    tx_head = (uint8_t)((tx_head + 1) & TX_BUF_MASK);
}

// Send buffered bytes until the transmitter is busy or the per-task budget is used up.
static void tx_flush(void)
{
    uint8_t i;

    for (i = 0; i < TX_BYTES_PER_TASK && tx_tail != tx_head; i++) {
        if (!remotecom_send_byte(tx_buf[tx_tail])) {
            return;
        }
        tx_tail = (uint8_t)((tx_tail + 1) & TX_BUF_MASK);
    }
}

static int8_t clamp_speed(int8_t v)
{
    if (v < -100) return -100;
    if (v > 100) return 100;
    return v;
}

static void uart_put_escaped(uint8_t b)
{
    if (b == RC_START || b == RC_ESC) {
        uart_put_raw(RC_ESC);
        uart_put_raw(b ^ RC_ESC_XOR);
    } else {
        uart_put_raw(b);
    }
}

static void send_frame(uint8_t msg_id, const uint8_t *payload, uint8_t len)
{
    uint16_t crc;
    uint8_t i;

    // Worst case every byte after START is escaped. Drop the whole frame rather than queue half of it.
    if (tx_free() < (uint8_t)(1 + 2 * (len + 4))) {
        return;
    }

    crc = 0xFFFF;
    crc = crc16_update(crc, msg_id);
    crc = crc16_update(crc, len);
    for (i = 0; i < len; i++) {
        crc = crc16_update(crc, payload[i]);
    }

    uart_put_raw(RC_START);
    uart_put_escaped(msg_id);
    uart_put_escaped(len);
    for (i = 0; i < len; i++) {
        uart_put_escaped(payload[i]);
    }
    uart_put_escaped((uint8_t)(crc >> 8));
    uart_put_escaped((uint8_t)(crc & 0xFF));
}

static void handle_frame(uint8_t msg_id, const uint8_t *payload, uint8_t len)
{
    switch (msg_id) {
    case MSG_REMOTE_CONTROL_RUN:
        if (len < REMOTE_CONTROL_RUN_LEN) {
            break;
        }
        remotecontrol_run(payload[3] != 0, clamp_speed((int8_t)payload[0]), clamp_speed((int8_t)payload[1]), clamp_speed((int8_t)payload[2]));
        break;
    case MSG_REMOTE_CONTROL_TURN:
        if (len < REMOTE_CONTROL_TURN_LEN) {
            break;
        }
        remotecontrol_turn(payload[4] != 0, clamp_speed((int8_t)payload[0]), payload[1], payload[2] != 0, clamp_speed((int8_t)payload[3]));
        break;
    default:
        break;
    }
}

static void rx_process_byte(uint8_t raw)
{
    uint8_t data;

    if (raw == RC_START) {
        // A raw START always (re)synchronizes to a new frame.
        rx_state = rx_msg_id;
        rx_escape_pending = false;
        rx_running_crc = 0xFFFF;
        return;
    }

    if (rx_state == rx_wait_start) {
        return;
    }

    if (rx_escape_pending) {
        data = raw ^ RC_ESC_XOR;
        rx_escape_pending = false;
    } else if (raw == RC_ESC) {
        rx_escape_pending = true;
        return;
    } else {
        data = raw;
    }

    switch (rx_state) {
    case rx_msg_id:
        rx_msg_id_byte = data;
        rx_running_crc = crc16_update(rx_running_crc, data);
        rx_state = rx_len;
        break;

    case rx_len:
        rx_len_byte = data;
        rx_running_crc = crc16_update(rx_running_crc, data);
        rx_payload_idx = 0;
        if (rx_len_byte > REMOTECOM_MAX_PAYLOAD) {
            rx_state = rx_wait_start; // payload too large for this build, drop the frame
        } else {
            rx_state = (rx_len_byte > 0) ? rx_payload : rx_crc_hi;
        }
        break;

    case rx_payload:
        rx_payload_buf[rx_payload_idx++] = data;
        rx_running_crc = crc16_update(rx_running_crc, data);
        if (rx_payload_idx >= rx_len_byte) {
            rx_state = rx_crc_hi;
        }
        break;

    case rx_crc_hi:
        rx_received_crc = (uint16_t)data << 8;
        rx_state = rx_crc_lo;
        break;

    case rx_crc_lo:
        rx_received_crc |= data;
        if (rx_received_crc == rx_running_crc) {
            handle_frame(rx_msg_id_byte, rx_payload_buf, rx_len_byte);
        }
        rx_state = rx_wait_start;
        break;

    default:
        rx_state = rx_wait_start;
        break;
    }
}

static uint8_t build_mower_status_payload(uint8_t *payload)
{
    uint8_t sensor_status;
    uint8_t wire_sensor_status;

    sensor_status = 0;
    if (get_sensor(SENSOR_FRONT)) { sensor_status |= 0x01; }
    if (get_sensor(SENSOR_LIFT))  { sensor_status |= 0x02; }

    wire_sensor_status = 0;
    if (get_sensor(SENSOR_RIGHT_WIRE_INSIDE)) { wire_sensor_status |= 0x01; }
    if (get_sensor(SENSOR_LEFT_WIRE_INSIDE))  { wire_sensor_status |= 0x02; }
    if (get_sensor(SENSOR_NEAR_WIRE))         { wire_sensor_status |= 0x04; }

    payload[0] = mowercontrol_get_state();
    payload[1] = sensor_status;
    payload[2] = wire_sensor_status;
    payload[3] = (uint8_t)get_requested_motor_speed(MOTOR_LEFT);
    payload[4] = (uint8_t)get_requested_motor_speed(MOTOR_RIGHT);
    payload[5] = get_battery_soc();
    return MOWER_STATUS_LEN;
}

// Send a status frame immediately and restart the periodic interval
void remotecom_send_status(void)
{
    uint8_t payload[MOWER_STATUS_LEN];

    build_mower_status_payload(payload);
    send_frame(MSG_MOWER_STATUS, payload, MOWER_STATUS_LEN);
    systimer_start(&status_send_timer, STATUS_SEND_INTERVAL_MS);
}

void init_remotecom(void)
{
    rx_state = rx_wait_start;
    rx_escape_pending = false;
    tx_head = 0;
    tx_tail = 0;

    systimer_start(&status_send_timer, STATUS_SEND_INTERVAL_MS);
}

void task_remotecom(void)
{
    uint8_t i;
    uint8_t rx_byte;

    for(i=0; i<10 && remotecom_recv_byte(&rx_byte) ; i++) {
        rx_process_byte(rx_byte);
    }

    if (systimer_is_expired(&status_send_timer)) {
        remotecom_send_status();
    }

    tx_flush();
}
