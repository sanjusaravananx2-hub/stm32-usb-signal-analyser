/**
 * @file    adc_capture.h
 * @brief   Timer-triggered ADC capture with DMA double-buffering
 */

#ifndef ADC_CAPTURE_H
#define ADC_CAPTURE_H

#include <stdint.h>

#define ADC_BUF_SIZE    2048
#define ADC_HALF_BUF    (ADC_BUF_SIZE / 2)

typedef enum {
    ADC_CH_PA0      = 0,
    ADC_CH_TEMP     = 1,
    ADC_CH_VREFINT  = 2,
} ADC_Channel_t;

typedef enum {
    CAPTURE_STOPPED = 0,
    CAPTURE_RUNNING,
    CAPTURE_SINGLE,
} ADC_CaptureState_t;

void               ADC_Capture_Init(void);
void               ADC_Capture_Start(void);
void               ADC_Capture_Stop(void);
void               ADC_Capture_Single(void);
uint32_t           ADC_Capture_SetRate(uint32_t rate_hz);
uint32_t           ADC_Capture_GetRate(void);
void               ADC_Capture_SetChannel(ADC_Channel_t ch);
ADC_Channel_t      ADC_Capture_GetChannel(void);
ADC_CaptureState_t ADC_Capture_GetState(void);
uint16_t          *ADC_Capture_GetReadyBuffer(void);
uint16_t          *ADC_Capture_GetRawBuffer(void);

#endif /* ADC_CAPTURE_H */
