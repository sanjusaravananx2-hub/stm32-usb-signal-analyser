/**
 * @file    adc_capture.c
 * @brief   Timer-triggered ADC capture with DMA double-buffering
 *
 * Hardware path: TIM2 TRGO --> ADC1 trigger --> DMA2 Stream0 (circular)
 * Half-transfer and transfer-complete DMA interrupts set ready flags.
 * App reads half-buffers via ADC_Capture_GetReadyBuffer().
 */

#include "adc_capture.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* CubeMX-generated HAL handles (defined in main.c) */
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;

/* DMA double-buffer (filled by DMA hardware, never touched by CPU during capture) */
static uint16_t adc_buf[ADC_BUF_SIZE];

/* Volatile ready flags — set in ISR, cleared in main loop */
static volatile uint8_t buf_half_ready = 0;
static volatile uint8_t buf_full_ready = 0;

/* State */
static volatile ADC_CaptureState_t capture_state = CAPTURE_STOPPED;
static ADC_Channel_t current_channel = ADC_CH_PA0;
static uint32_t current_rate_hz = 10000;

/* ---------------------------------------------------------------------------
 * Private helpers
 * --------------------------------------------------------------------------- */

static void start_dma(void)
{
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, ADC_BUF_SIZE);
    HAL_TIM_Base_Start(&htim2);
}

static void stop_dma(void)
{
    HAL_TIM_Base_Stop(&htim2);
    HAL_ADC_Stop_DMA(&hadc1);
}

/* ===========================================================================
 * Public API
 * =========================================================================== */

void ADC_Capture_Init(void)
{
    buf_half_ready  = 0;
    buf_full_ready  = 0;
    capture_state   = CAPTURE_STOPPED;
    current_channel = ADC_CH_PA0;
    current_rate_hz = 10000;
    memset(adc_buf, 0, sizeof(adc_buf));
}

void ADC_Capture_Start(void)
{
    if (capture_state != CAPTURE_STOPPED) {
        stop_dma();
    }
    buf_half_ready = 0;
    buf_full_ready = 0;
    capture_state  = CAPTURE_RUNNING;
    start_dma();
}

void ADC_Capture_Stop(void)
{
    stop_dma();
    capture_state = CAPTURE_STOPPED;
}

void ADC_Capture_Single(void)
{
    if (capture_state != CAPTURE_STOPPED) {
        stop_dma();
    }
    buf_half_ready = 0;
    buf_full_ready = 0;
    capture_state  = CAPTURE_SINGLE;
    start_dma();
}

uint32_t ADC_Capture_SetRate(uint32_t rate_hz)
{
    if (rate_hz < 1)       rate_hz = 1;
    if (rate_hz > 500000)  rate_hz = 500000;

    /* TIM2 clock = APB1 timer clock = 96 MHz (APB1 prescaler=2, timer×2) */
    const uint32_t tim_clk = 96000000UL;
    uint32_t total_div = tim_clk / rate_hz;
    if (total_div < 1) total_div = 1;

    /* Choose PSC so ARR fits in 16 bits */
    uint32_t psc = 0;
    uint32_t arr = total_div - 1;
    while (arr > 0xFFFF && psc < 0xFFFF) {
        psc++;
        arr = (total_div / (psc + 1)) - 1;
    }

    uint8_t was_running = (capture_state != CAPTURE_STOPPED);
    if (was_running) stop_dma();

    htim2.Init.Prescaler = psc;
    htim2.Init.Period    = arr;
    HAL_TIM_Base_Init(&htim2);

    if (was_running) start_dma();

    current_rate_hz = tim_clk / ((psc + 1) * (arr + 1));
    return current_rate_hz;
}

uint32_t ADC_Capture_GetRate(void)
{
    return current_rate_hz;
}

void ADC_Capture_SetChannel(ADC_Channel_t ch)
{
    uint32_t hal_ch;
    switch (ch) {
        case ADC_CH_TEMP:    hal_ch = ADC_CHANNEL_TEMPSENSOR; break;
        case ADC_CH_VREFINT: hal_ch = ADC_CHANNEL_VREFINT;    break;
        default:             hal_ch = ADC_CHANNEL_0;           break;
    }

    ADC_ChannelConfTypeDef cfg = {0};
    cfg.Channel      = hal_ch;
    cfg.Rank         = 1;
    cfg.SamplingTime = ADC_SAMPLETIME_84CYCLES;

    uint8_t was_running = (capture_state != CAPTURE_STOPPED);
    if (was_running) stop_dma();

    HAL_ADC_ConfigChannel(&hadc1, &cfg);
    current_channel = ch;

    if (was_running) start_dma();
}

ADC_Channel_t ADC_Capture_GetChannel(void)
{
    return current_channel;
}

ADC_CaptureState_t ADC_Capture_GetState(void)
{
    return capture_state;
}

uint16_t *ADC_Capture_GetReadyBuffer(void)
{
    if (buf_half_ready) {
        buf_half_ready = 0;
        return &adc_buf[0];
    }
    if (buf_full_ready) {
        buf_full_ready = 0;
        if (capture_state == CAPTURE_SINGLE) {
            stop_dma();
            capture_state = CAPTURE_STOPPED;
        }
        return &adc_buf[ADC_HALF_BUF];
    }
    return NULL;
}

uint16_t *ADC_Capture_GetRawBuffer(void)
{
    return adc_buf;
}

/* ===========================================================================
 * DMA interrupt callbacks (called by HAL from DMA2_Stream0_IRQHandler)
 * =========================================================================== */

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        buf_half_ready = 1;
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        buf_full_ready = 1;
    }
}
