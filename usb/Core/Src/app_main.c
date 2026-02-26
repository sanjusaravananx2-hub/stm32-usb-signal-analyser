/**
 * @file    app_main.c
 * @brief   Application main loop for the STM32 USB Signal Analyzer
 *
 * Call App_Init() once after all MX_xxx_Init() calls.
 * Call App_MainLoop() from while(1) in main().
 */

#include "adc_capture.h"
#include "cmd_handler.h"
#include "stm32f4xx_hal.h"

#define LED_PORT        GPIOC
#define LED_PIN         GPIO_PIN_13
#define HEARTBEAT_MS    500
#define USB_SETTLE_MS   1500

static uint32_t heartbeat_last_tick = 0;
static uint8_t  welcome_sent        = 0;
static uint32_t startup_tick        = 0;

void App_Init(void)
{
    ADC_Capture_Init();
    CMD_Init();
    startup_tick        = HAL_GetTick();
    heartbeat_last_tick = startup_tick;
    welcome_sent        = 0;
    ADC_Capture_SetRate(10000);
}

void App_MainLoop(void)
{
    uint32_t now = HAL_GetTick();

    /* 1. Send welcome banner once after USB has enumerated */
    if (!welcome_sent && (now - startup_tick >= USB_SETTLE_MS)) {
        CMD_SendText("");
        CMD_SendText("=== STM32 USB Signal Analyzer v1.0.0 ===");
        CMD_SendText("Type 'help' for available commands.");
        CMD_SendText("");
        welcome_sent = 1;
    }

    /* 2. Stream ADC data while capture is running */
    if (ADC_Capture_GetState() != CAPTURE_STOPPED) {
        uint16_t *buf = ADC_Capture_GetReadyBuffer();
        if (buf != NULL) {
            CMD_SendDataFrame(buf, ADC_HALF_BUF);
        }
    }

    /* 3. Catch final buffer from single-shot capture */
    {
        uint16_t *buf = ADC_Capture_GetReadyBuffer();
        if (buf != NULL && ADC_Capture_GetState() == CAPTURE_STOPPED) {
            CMD_SendDataFrame(buf, ADC_HALF_BUF);
        }
    }

    /* 4. USB retry handling */
    CMD_Tick();

    /* 5. Heartbeat LED on PC13 (active low on Black Pill) */
    if (now - heartbeat_last_tick >= HEARTBEAT_MS) {
        heartbeat_last_tick = now;
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
    }
}
