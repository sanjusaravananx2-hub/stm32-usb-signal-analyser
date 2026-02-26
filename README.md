# STM32 USB Signal Analyzer

A real-time USB oscilloscope and FFT spectrum analyser built on the STM32F411CEU6 (Black Pill).
The firmware streams 12-bit ADC samples over USB CDC at up to 500 kHz.
A Python host application displays live waveform and frequency spectrum.

![Live oscilloscope screenshot](docs/screenshot.png)

---

## Features

- **Real-time waveform display** — 0 to 3.3 V, configurable sample rate
- **FFT frequency spectrum** — identifies dominant frequencies in the signal
- **Zero CPU overhead sampling** — TIM2 → ADC1 → DMA2 double-buffer pipeline
- **USB CDC streaming** — appears as a virtual COM port, no drivers needed
- **Binary framing with CRC-8** — reliable data integrity over USB
- **Interactive commands** — set sample rate, select channel, single-shot capture
- **Heartbeat LED** — PC13 blinks at 0.5 Hz to confirm firmware is running

---

## Skills Demonstrated

| Skill | Detail |
|---|---|
| Bare-metal C firmware | STM32F411, HAL, peripheral drivers |
| Timer-triggered ADC | TIM2 TRGO → ADC1 at configurable rate |
| DMA double-buffering | DMA2 Stream0 circular, zero-copy streaming |
| USB CDC (Virtual COM Port) | USB OTG FS, STM32 USB Device Library |
| Binary communication protocol | Sync bytes, sequence numbers, CRC-8/SMBUS |
| Real-time signal processing | FFT with Hanning window, dB spectrum |
| Python instrumentation | pyserial, matplotlib, numpy, threading |
| Interrupt-driven design | DMA half/complete callbacks, NVIC |

---

## Hardware

| Component | Details |
|---|---|
| MCU board | STM32F411CEU6 Black Pill (WeAct) |
| Clock | 25 MHz HSE crystal → 96 MHz SYSCLK via PLL |
| ADC input | PA0 (ADC1_IN0), 12-bit, 0–3.3 V |
| USB | USB OTG FS on PA11/PA12 (USB-C connector) |
| LED | PC13 (active low, heartbeat) |

---

## System Architecture

```
                    STM32F411CEU6 Black Pill
┌─────────────────────────────────────────────────────────┐
│                                                         │
│  PA0 ──► ADC1 ──► DMA2 Stream0 ──► RAM buffer (2048)   │
│          ▲              │                               │
│        TIM2             │ Half/Complete IRQ             │
│       (10 kHz)          ▼                               │
│                   app_main.c                            │
│                   GetReadyBuffer()                      │
│                         │                               │
│                         ▼                               │
│                   cmd_handler.c                         │
│                   SendDataFrame()                       │
│                         │                               │
│                         ▼                               │
│                   USB CDC (Virtual COM Port)            │
└─────────────────────────┬───────────────────────────────┘
                          │ USB-C
                          ▼
               host/oscilloscope.py
               ├── FrameReader (background thread)
               │   └── CRC-8 verification
               └── Oscilloscope (matplotlib)
                   ├── Waveform plot (voltage vs time)
                   └── FFT plot (dB vs frequency)
```

---

## Binary Frame Protocol

```
Byte:  0     1     2      3       4       5      6…N    N+1
      [0xAA][0x55][type][len_lo][len_hi][seq][...data...][CRC8]
```

| Field | Value | Description |
|---|---|---|
| Sync | `0xAA 0x55` | Frame start marker |
| Type | `0x01` | ADC data frame |
| Length | uint16 LE | Number of data bytes (samples × 2) |
| Sequence | uint8 | Increments each frame, detects drops |
| Data | uint16 LE × N | Raw 12-bit ADC samples |
| CRC-8 | uint8 | CRC-8/SMBUS over type+len+seq+data |

---

## Project Structure

```
stm32-usb-signal-analyzer/
├── Core/
│   ├── Inc/
│   │   ├── adc_capture.h      # ADC + DMA engine API
│   │   └── cmd_handler.h      # USB CDC command handler API
│   └── Src/
│       ├── adc_capture.c      # TIM2 → ADC1 → DMA2 driver
│       ├── cmd_handler.c      # Command parser + binary framer
│       ├── app_main.c         # Top-level app loop
│       └── main.c             # CubeMX-generated entry point
├── Drivers/                   # STM32 HAL (generated)
├── Middlewares/               # USB Device Library (generated)
├── USB_DEVICE/                # CDC class implementation (generated)
├── host/
│   └── oscilloscope.py        # Python real-time oscilloscope
├── docs/
│   └── setup.md               # Full build guide
└── usb-signal-analyser.ioc    # STM32CubeMX configuration
```

---

## Firmware Commands

| Command | Description |
|---|---|
| `start` | Start continuous ADC capture and streaming |
| `stop` | Stop capture |
| `single` | Capture one buffer (1024 samples) then stop |
| `rate <Hz>` | Set sample rate, e.g. `rate 44100` |
| `channel 0` | PA0 — external signal input |
| `channel 1` | Internal temperature sensor |
| `channel 2` | Internal voltage reference (~1.21 V) |
| `status` | Show current rate, channel, state |
| `id` | Show firmware version |
| `help` | List all commands |

---

## Build Instructions

### Requirements
- STM32CubeIDE 2.x
- STM32CubeMX (for peripheral reconfiguration)
- Python 3.8+: `pip3 install pyserial matplotlib numpy`

### Flash Firmware
1. Open the project in STM32CubeIDE
2. Build: **Cmd+B** (macOS) or **Ctrl+B** (Windows)
3. Enter DFU mode: hold BOOT0, press/release NRST, release BOOT0
4. Flash using STM32CubeProgrammer (USB/DFU connection)
5. Press NRST to reset and run

### Run Oscilloscope
```bash
cd host
python3 oscilloscope.py                        # auto-detect port
python3 oscilloscope.py --port /dev/tty.usbmodem1103
python3 oscilloscope.py --port COM3 --rate 44100
```

---

## Clock Configuration (STM32F411CEU6)

| Parameter | Value |
|---|---|
| HSE | 25 MHz (Black Pill crystal) |
| PLLM | 25 |
| PLLN | 192 |
| PLLP | 2 |
| PLLQ | 4 |
| SYSCLK | 96 MHz |
| USB clock | 48 MHz |
| APB1 | 48 MHz |
| TIM2 clock | 96 MHz |

---

## Author

**Sanjeev Kumar**
MSc Embedded Systems Engineering, University of Leeds (2025–2026)

GitHub: [sanjusaravananx2](https://github.com/sanjusaravananx2)
LinkedIn: [linkedin.com/in/sanjeev-kumarx2](https://linkedin.com/in/sanjeev-kumarx2)
