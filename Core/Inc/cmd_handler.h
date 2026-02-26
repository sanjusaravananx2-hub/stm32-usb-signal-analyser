/**
 * @file    cmd_handler.h
 * @brief   USB CDC command parser and response handler
 */

#ifndef CMD_HANDLER_H
#define CMD_HANDLER_H

#include <stdint.h>

#define FRAME_SYNC_0            0xAA
#define FRAME_SYNC_1            0x55
#define FRAME_TYPE_ADC_DATA     0x01
#define FRAME_TYPE_TEXT_RESP    0x02

void CMD_Init(void);
void CMD_ProcessInput(uint8_t *buf, uint32_t len);
void CMD_SendDataFrame(uint16_t *samples, uint16_t count);
void CMD_SendText(const char *text);
void CMD_Tick(void);

#endif /* CMD_HANDLER_H */
