/**
 * @file    cmd_handler.c
 * @brief   USB CDC command parser and response handler
 *
 * Text commands arrive over USB CDC. Binary ADC frames are sent back.
 *
 * Binary frame format:
 *   [0xAA][0x55][type:1][len_lo:1][len_hi:1][seq:1][data:len][crc8:1]
 *   CRC-8/SMBUS over: type + len_lo + len_hi + seq + data
 */

#include "cmd_handler.h"
#include "adc_capture.h"
#include "usbd_cdc_if.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------
 * Configuration
 * --------------------------------------------------------------------------- */
#define LINE_BUF_SIZE       128
#define RETRY_BUF_SIZE      (6 + ADC_HALF_BUF * 2 + 1)
#define RETRY_TIMEOUT_MS    5
#define CRC8_POLY           0x07

/* ---------------------------------------------------------------------------
 * Module state
 * --------------------------------------------------------------------------- */
static uint8_t  line_buf[LINE_BUF_SIZE];
static uint8_t  line_len   = 0;
static uint8_t  frame_seq  = 0;

/* Retry buffer for USB BUSY handling */
static uint8_t  retry_buf[RETRY_BUF_SIZE];
static uint16_t retry_len      = 0;
static uint32_t retry_last_tick = 0;

/* ---------------------------------------------------------------------------
 * Private: CRC-8/SMBUS
 * --------------------------------------------------------------------------- */
static uint8_t crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ CRC8_POLY) & 0xFF
                               : (crc << 1) & 0xFF;
        }
    }
    return crc;
}

/* ---------------------------------------------------------------------------
 * Private: USB transmit with retry
 * --------------------------------------------------------------------------- */
static void usb_send(const uint8_t *data, uint16_t len)
{
    if (len == 0) return;
    if (CDC_Transmit_FS((uint8_t *)data, len) == USBD_BUSY) {
        if (retry_len == 0 && len <= RETRY_BUF_SIZE) {
            memcpy(retry_buf, data, len);
            retry_len       = len;
            retry_last_tick = HAL_GetTick();
        }
    }
}

/* ---------------------------------------------------------------------------
 * Private: command handlers
 * --------------------------------------------------------------------------- */
static void cmd_help(void)
{
    CMD_SendText("Commands:");
    CMD_SendText("  start           - Start continuous ADC capture");
    CMD_SendText("  stop            - Stop capture");
    CMD_SendText("  single          - Capture one buffer then stop");
    CMD_SendText("  rate <Hz>       - Set sample rate (1 to 500000)");
    CMD_SendText("  channel <0-2>   - 0=PA0  1=temperature  2=Vrefint");
    CMD_SendText("  status          - Show current settings");
    CMD_SendText("  id              - Show firmware version");
    CMD_SendText("  help            - Show this help");
}

static void cmd_start(void)
{
    ADC_Capture_Start();
    CMD_SendText("OK capture started");
}

static void cmd_stop(void)
{
    ADC_Capture_Stop();
    CMD_SendText("OK capture stopped");
}

static void cmd_single(void)
{
    ADC_Capture_Single();
    CMD_SendText("OK single capture started");
}

static void cmd_rate(const char *arg)
{
    if (!arg || *arg == '\0') { CMD_SendText("ERR usage: rate <Hz>"); return; }
    uint32_t actual = ADC_Capture_SetRate((uint32_t)atoi(arg));
    char buf[64];
    snprintf(buf, sizeof(buf), "OK rate %lu Hz", actual);
    CMD_SendText(buf);
}

static void cmd_channel(const char *arg)
{
    if (!arg || *arg == '\0') { CMD_SendText("ERR usage: channel <0-2>"); return; }
    int ch = atoi(arg);
    if (ch < 0 || ch > 2) { CMD_SendText("ERR channel 0-2 only"); return; }
    ADC_Capture_SetChannel((ADC_Channel_t)ch);
    const char *names[] = {"PA0 (external)", "temperature sensor", "Vrefint"};
    char buf[64];
    snprintf(buf, sizeof(buf), "OK channel %s", names[ch]);
    CMD_SendText(buf);
}

static void cmd_status(void)
{
    char buf[96];
    const char *st;
    switch (ADC_Capture_GetState()) {
        case CAPTURE_RUNNING: st = "RUNNING"; break;
        case CAPTURE_SINGLE:  st = "SINGLE";  break;
        default:              st = "STOPPED"; break;
    }
    const char *ch;
    switch (ADC_Capture_GetChannel()) {
        case ADC_CH_TEMP:    ch = "TEMP"; break;
        case ADC_CH_VREFINT: ch = "VREF"; break;
        default:             ch = "PA0";  break;
    }
    snprintf(buf, sizeof(buf), "State:%s  Rate:%lu Hz  Ch:%s  BufSz:%d",
             st, ADC_Capture_GetRate(), ch, ADC_HALF_BUF);
    CMD_SendText(buf);
}

static void cmd_id(void)
{
    CMD_SendText("STM32 USB Signal Analyzer v1.0.0 (STM32F411CEU6)");
}

/* ---------------------------------------------------------------------------
 * Private: process one complete text line
 * --------------------------------------------------------------------------- */
static void process_line(uint8_t *buf, uint8_t len)
{
    if (len >= LINE_BUF_SIZE) len = LINE_BUF_SIZE - 1;
    buf[len] = '\0';

    char *cmd = (char *)buf;
    while (*cmd == ' ') cmd++;

    char *arg = cmd;
    while (*arg && *arg != ' ') arg++;
    if (*arg == ' ') { *arg = '\0'; arg++; }
    while (*arg == ' ') arg++;

    if      (!strcmp(cmd, "help"))    cmd_help();
    else if (!strcmp(cmd, "start"))   cmd_start();
    else if (!strcmp(cmd, "stop"))    cmd_stop();
    else if (!strcmp(cmd, "single"))  cmd_single();
    else if (!strcmp(cmd, "rate"))    cmd_rate(arg);
    else if (!strcmp(cmd, "channel")) cmd_channel(arg);
    else if (!strcmp(cmd, "status"))  cmd_status();
    else if (!strcmp(cmd, "id"))      cmd_id();
    else if (*cmd != '\0') {
        char resp[64];
        snprintf(resp, sizeof(resp), "ERR unknown: '%s' (try 'help')", cmd);
        CMD_SendText(resp);
    }
}

/* ===========================================================================
 * Public API
 * =========================================================================== */

void CMD_Init(void)
{
    line_len   = 0;
    retry_len  = 0;
    frame_seq  = 0;
    memset(line_buf, 0, sizeof(line_buf));
}

void CMD_ProcessInput(uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        uint8_t c = buf[i];
        if (c == '\n' || c == '\r') {
            if (line_len > 0) {
                process_line(line_buf, line_len);
                line_len = 0;
            }
        } else if (line_len < LINE_BUF_SIZE - 1) {
            line_buf[line_len++] = c;
        }
    }
}

void CMD_SendDataFrame(uint16_t *samples, uint16_t count)
{
    uint16_t data_bytes = count * 2;
    /* Static buffer: header(6) + data + crc(1) */
    static uint8_t frame[6 + ADC_HALF_BUF * 2 + 1];
    uint16_t frame_len = 6 + data_bytes + 1;

    if (frame_len > sizeof(frame)) return;

    frame[0] = FRAME_SYNC_0;
    frame[1] = FRAME_SYNC_1;
    frame[2] = FRAME_TYPE_ADC_DATA;
    frame[3] = data_bytes & 0xFF;
    frame[4] = (data_bytes >> 8) & 0xFF;
    frame[5] = frame_seq++;

    memcpy(&frame[6], samples, data_bytes);
    frame[6 + data_bytes] = crc8(&frame[2], 4 + data_bytes);

    usb_send(frame, frame_len);
}

void CMD_SendText(const char *text)
{
    static uint8_t tx_buf[256];
    uint16_t len = (uint16_t)strlen(text);
    if (len > sizeof(tx_buf) - 2) len = sizeof(tx_buf) - 2;
    memcpy(tx_buf, text, len);
    tx_buf[len]     = '\r';
    tx_buf[len + 1] = '\n';
    usb_send(tx_buf, len + 2);
}

void CMD_Tick(void)
{
    if (retry_len == 0) return;
    if (HAL_GetTick() - retry_last_tick < RETRY_TIMEOUT_MS) return;

    if (CDC_Transmit_FS(retry_buf, retry_len) != USBD_BUSY) {
        retry_len = 0;
    } else {
        retry_last_tick = HAL_GetTick();
    }
}
