# STM32 USB Signal Analyzer — Complete Build Guide
## macOS (MacBook M2) + STM32F411CEU6 Black Pill

---

## What you will build
The Black Pill samples voltage on pin PA0 at up to 500 kHz, streams data to
your Mac over USB-C, and Python displays a live waveform + FFT on screen.

---

## PART 1 — Install software

### 1.1 STM32CubeIDE
1. Go to **st.com/stm32cubeide** in Safari or Chrome
2. Click the **macOS** installer (`.dmg`, ~1 GB)
3. You need to create a free ST account to download — sign up
4. Open the downloaded `.dmg` and drag STM32CubeIDE into Applications
5. First launch: macOS may block it → go to  **System Settings → Privacy & Security**,
   scroll down, click **Open Anyway** next to the STM32CubeIDE message

### 1.2 Python packages (for the oscilloscope display)
Open **Terminal** and run:
```bash
pip3 install pyserial matplotlib numpy
```
If `pip3` is not found, install Python from **python.org/downloads** first
(tick "Add to PATH" during install), then run the command again.

---

## PART 2 — Create the STM32CubeIDE project

1. Open **STM32CubeIDE**
2. Choose a workspace (e.g. `/Users/YourName/STM32Projects`) → click Launch
3. Click **File → New → STM32 Project**
4. A dialog titled **"Create / Import STM32 Project"** appears
5. Click **"STM32CubeIDE Empty Project"** to highlight it
6. Click **Next >** (NOT Finish — clicking Next takes you to the chip selector)
7. On the next page:
   - Project Name: `usb-signal-analyser`
   - In the MCU search box, type: `STM32F411CEU6`
   - Select **STM32F411CEU6** from the list
   - Language: **C**
8. Click **Finish**
9. If asked "Open associated perspective?" → click **Yes**
10. The `.ioc` chip diagram opens — you see a picture of the chip with pins around it ✓

> **If step 6 did NOT show a chip search box:** take a screenshot and send it —
> we will fix it before continuing.

---

## PART 3 — Configure the chip (.ioc diagram)

You are now in the CubeMX configurator. Work through each section.

### 3.1 RCC — clock source
- Left panel → expand **System Core** → click **RCC**
- High Speed Clock (HSE): **Crystal/Ceramic Resonator**
- Low Speed Clock (LSE): leave as **Disable**

### 3.2 USB — enable USB peripheral
- Left panel → expand **Connectivity** → click **USB_OTG_FS**
- Mode: **Device_Only**
- Leave everything else as default

### 3.3 USB Middleware — Virtual COM Port
- Left panel → expand **Middleware and Software Packs** → click **USB_DEVICE**
- Class For FS IP: **Communication Device Class (Virtual Port Com)**

### 3.4 ADC — analogue input
- Left panel → expand **Analog** → click **ADC1**
- Tick **IN0** (enables pin PA0 as analogue input — the signal you'll measure)
- In the **Configuration** panel at the bottom, set:
  - Resolution: **12 bits**
  - Scan Conversion Mode: **Disabled**
  - Continuous Conversion Mode: **Disabled**
  - DMA Continuous Requests: **Enabled**
  - External Trigger Conversion Source: **Timer 2 Trigger Out event**
  - External Trigger Conversion Edge: **Trigger detection on the rising edge**
  - Data Alignment: **Right alignment**

### 3.5 TIM2 — sets the sample rate
- Left panel → expand **Timers** → click **TIM2**
- Clock Source: **Internal Clock**
- In the **Configuration** panel:
  - Prescaler: `95`
  - Counter Period (ARR): `99`
  - Trigger Event Selection: **Update Event**
  - *(96 MHz ÷ 96 ÷ 100 = 10,000 Hz sample rate)*

### 3.6 DMA — move ADC data automatically
- Still in ADC1 configuration, click the **DMA Settings** tab
- Click **Add**
- Select **ADC1**
- Direction: **Peripheral To Memory**
- Mode: **Circular**
- Peripheral Data Width: **Half Word** | Memory Data Width: **Half Word**

### 3.7 GPIO — heartbeat LED
- On the chip diagram, find pin **PC13** and click it
- Select **GPIO_Output**
- In the GPIO Configuration panel, set User Label: `LED`

### 3.8 NVIC — enable interrupts
- Left panel → **System Core** → **NVIC**
- Ensure these boxes are **ticked**:
  - `DMA2 stream0 global interrupt`
  - `USB OTG FS global interrupt`

### 3.9 Clock Configuration — set 96 MHz
- Click the **Clock Configuration** tab at the top of the .ioc editor
- Set the following values (click each box and type the number):
  | Setting | Value |
  |---|---|
  | Input frequency | `25` |
  | PLL Source Mux | HSE |
  | PLLM | `25` |
  | PLLN | `192` |
  | PLLP | `4` |
  | PLLQ | `8` |
  | System Clock Mux | PLLCLK |
  | APB1 Prescaler | `/2` |
  | APB2 Prescaler | `/1` |
- SYSCLK should read **96 MHz** and USB clock **48 MHz**
- If you see any red errors → click **Resolve Clock Issues** button

### 3.10 Project Manager settings
- Click the **Project Manager** tab at the top
- Click **Code Generator** on the left
- Tick: **Generate peripheral initialization as a pair of .c/.h files per peripheral**
- Tick: **Keep User Code when re-generating**

### 3.11 Generate code
- Press **Cmd+S** to save and generate code
- Click **Yes** if asked to generate
- Click **Yes** if asked to open C/C++ perspective
- In Project Explorer you will now see folders: `Core/`, `Drivers/`, `USB_DEVICE/`

---

## PART 4 — Add the firmware files

### 4.1 Find the project folder in Finder
The project is inside your workspace. In Finder, navigate to:
```
/Users/YourName/STM32Projects/usb-signal-analyser/
```

### 4.2 Copy the 5 firmware files

From `/Users/sanjeevkumar/MyCareer/stm32-usb-signal-analyzer/firmware/`:

| Copy this file | Into this folder |
|---|---|
| `Core/Src/adc_capture.c` | `Core/Src/` |
| `Core/Src/cmd_handler.c` | `Core/Src/` |
| `Core/Src/app_main.c`    | `Core/Src/` |
| `Core/Inc/adc_capture.h` | `Core/Inc/` |
| `Core/Inc/cmd_handler.h` | `Core/Inc/` |

After copying, go to STM32CubeIDE → right-click the project → **Refresh**
(or press **F5**). The files will appear in Project Explorer.

### 4.3 Edit main.c

Open `Core/Src/main.c` in STM32CubeIDE.

**Edit 1** — Add includes (around line 25):
```c
/* USER CODE BEGIN Includes */
#include "adc_capture.h"
#include "cmd_handler.h"
void App_Init(void);
void App_MainLoop(void);
/* USER CODE END Includes */
```

**Edit 2** — Call App_Init after peripheral init (find `USER CODE BEGIN 2`):
```c
/* USER CODE BEGIN 2 */
App_Init();
/* USER CODE END 2 */
```

**Edit 3** — Call App_MainLoop inside while(1) (find `USER CODE BEGIN 3`):
```c
/* USER CODE BEGIN 3 */
App_MainLoop();
/* USER CODE END 3 */
```

### 4.4 Edit usbd_cdc_if.c

Open `USB_DEVICE/App/usbd_cdc_if.c`

Find the `CDC_Receive_FS` function. Replace the contents between the USER CODE
comments so it looks exactly like this:

```c
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */
  extern void CMD_ProcessInput(uint8_t *buf, uint32_t len);
  CMD_ProcessInput(Buf, *Len);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (USBD_OK);
  /* USER CODE END 6 */
}
```

---

## PART 5 — Build and flash

### 5.1 Build
- Press **Cmd+B**
- Watch the **Console** panel at the bottom
- Success looks like:
  ```
  Build Finished. 0 errors, 0 warnings.
  ```
- If there are errors → screenshot the Console and send it

### 5.2 Connect the Black Pill
- Plug the Black Pill into your MacBook using a **USB-C to USB-C cable**
- macOS may show a "new accessory detected" notification — click OK

### 5.3 Flash the firmware
- Click the green **Run** button (▶) in the toolbar
  OR go to **Run → Run**
- First time: a "Run Configurations" dialog may open → just click **OK**
- Flashing takes about 5 seconds
- When done, the Black Pill's onboard LED starts **blinking every 0.5 seconds** ✓

---

## PART 6 — Run the Python oscilloscope

Open **Terminal** and run:
```bash
cd /Users/sanjeevkumar/MyCareer/stm32-usb-signal-analyzer/host
python3 oscilloscope.py
```

If it says "No STM32 device found", find the port manually:
```bash
ls /dev/tty.usbmodem*
```
This shows something like `/dev/tty.usbmodem1103`. Then run:
```bash
python3 oscilloscope.py --port /dev/tty.usbmodem1103
```

A window opens with:
- **Top graph**: live voltage waveform (0–3.3V)
- **Bottom graph**: FFT frequency spectrum

**Quick test**: touch pin PA0 on the Black Pill with your finger.
You will see 50 Hz mains interference appear as a spike in the FFT. That means everything works.

---

## Commands you can type (in any serial terminal)

| Command | What it does |
|---|---|
| `start` | Start continuous capture + streaming |
| `stop` | Stop capture |
| `single` | Capture one buffer (1024 samples) then stop |
| `rate 44100` | Change sample rate to 44100 Hz |
| `channel 0` | Measure PA0 (external signal) |
| `channel 1` | Measure internal temperature sensor |
| `channel 2` | Measure internal voltage reference |
| `status` | Show current rate, channel, state |
| `help` | List all commands |

---

## Troubleshooting

| Problem | Fix |
|---|---|
| CubeIDE blocked by macOS | System Settings → Privacy & Security → Open Anyway |
| Step 6 (Next) did not show chip search | Screenshot it and send — we will fix it |
| Build error: `hadc1` undeclared | ADC1 was not enabled in step 3.4 — redo it |
| Build error: `usbd_cdc_if.h` not found | USB_DEVICE middleware missing — redo step 3.3 |
| LED not blinking after flash | Try pressing NRST button on Black Pill after flash |
| `ls /dev/tty.usbmodem*` shows nothing | Try a different cable (some USB-C cables are charge-only) |
| Plot opens but no data | The Python script sends `start` automatically; if no data, type `start` in a serial terminal |
