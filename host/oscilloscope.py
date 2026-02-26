#!/usr/bin/env python3
"""
STM32 USB Signal Analyzer — Real-Time Oscilloscope + FFT
=========================================================
Usage:
    pip install pyserial matplotlib numpy
    python oscilloscope.py                   # Auto-detect COM port
    python oscilloscope.py --port COM3
    python oscilloscope.py --port COM3 --rate 44100
"""

import argparse
import sys
import time
import threading
from collections import deque

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import serial
import serial.tools.list_ports

SYNC_0         = 0xAA
SYNC_1         = 0x55
FRAME_TYPE_ADC = 0x01
CRC8_POLY      = 0x07


def crc8_smbus(data: bytes) -> int:
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ CRC8_POLY) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def find_stm32_port() -> str:
    for p in serial.tools.list_ports.comports():
        if (p.vid or 0) == 0x0483:
            return p.device
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").lower()
        if "cdc" in desc or "stm" in desc or "acm" in desc or "virtual" in desc:
            return p.device
    ports = serial.tools.list_ports.comports()
    return ports[0].device if ports else None


class FrameReader:
    def __init__(self, ser: serial.Serial):
        self.ser          = ser
        self.frames       = deque(maxlen=10)
        self.dropped      = 0
        self.total_frames = 0
        self.last_seq     = -1
        self._running     = False
        self._thread      = None

    def start(self):
        self._running = True
        self._thread  = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        if self._thread:
            self._thread.join(timeout=2)

    def _read_loop(self):
        buf = bytearray()
        while self._running:
            try:
                chunk = self.ser.read(self.ser.in_waiting or 1)
                if not chunk:
                    continue
                buf.extend(chunk)

                while len(buf) >= 6:
                    idx = self._find_sync(buf)
                    if idx < 0:
                        buf = buf[-1:]
                        break
                    if idx > 0:
                        buf = buf[idx:]
                    if len(buf) < 6:
                        break

                    frame_type = buf[2]
                    data_len   = buf[3] | (buf[4] << 8)
                    seq        = buf[5]
                    total_len  = 6 + data_len + 1

                    if len(buf) < total_len:
                        break

                    frame_data = bytes(buf[6:6 + data_len])
                    frame_crc  = buf[6 + data_len]
                    expected   = crc8_smbus(bytes(buf[2:6 + data_len]))
                    buf        = buf[total_len:]

                    if frame_crc != expected:
                        self.dropped += 1
                        continue
                    if frame_type != FRAME_TYPE_ADC:
                        continue

                    if self.last_seq >= 0:
                        expected_seq = (self.last_seq + 1) & 0xFF
                        if seq != expected_seq:
                            self.dropped += (seq - expected_seq) & 0xFF
                    self.last_seq = seq

                    n       = data_len // 2
                    samples = np.frombuffer(frame_data[:n * 2], dtype=np.uint16)
                    self.frames.append(samples)
                    self.total_frames += 1

            except serial.SerialException:
                break
            except Exception as e:
                print(f"Reader error: {e}")
                time.sleep(0.01)

    @staticmethod
    def _find_sync(buf: bytearray) -> int:
        for i in range(len(buf) - 1):
            if buf[i] == SYNC_0 and buf[i + 1] == SYNC_1:
                return i
        return -1

    def get_latest(self):
        return self.frames[-1] if self.frames else None


class Oscilloscope:
    def __init__(self, reader: FrameReader, sample_rate: int):
        self.reader      = reader
        self.sample_rate = sample_rate
        self.adc_to_v    = 3.3 / 4095.0

        self.fig, (self.ax_wave, self.ax_fft) = plt.subplots(2, 1, figsize=(12, 7))
        self.fig.suptitle("STM32 USB Signal Analyzer", fontsize=14, fontweight="bold")

        self.ax_wave.set_title("Waveform")
        self.ax_wave.set_xlabel("Time (ms)")
        self.ax_wave.set_ylabel("Voltage (V)")
        self.ax_wave.set_ylim(-0.1, 3.4)
        self.ax_wave.grid(True, alpha=0.3)
        (self.line_wave,) = self.ax_wave.plot([], [], "c-", linewidth=0.8)

        self.ax_fft.set_title("Frequency Spectrum")
        self.ax_fft.set_xlabel("Frequency (Hz)")
        self.ax_fft.set_ylabel("Magnitude (dB)")
        self.ax_fft.set_ylim(-80, 5)
        self.ax_fft.grid(True, alpha=0.3)
        (self.line_fft,) = self.ax_fft.plot([], [], "m-", linewidth=0.8)

        self.status = self.ax_wave.text(
            0.02, 0.95, "", transform=self.ax_wave.transAxes,
            fontsize=9, verticalalignment="top", fontfamily="monospace",
            bbox=dict(boxstyle="round", facecolor="black", alpha=0.8), color="lime"
        )
        self.fig.tight_layout(rect=[0, 0, 1, 0.96])

    def update(self, _):
        samples = self.reader.get_latest()
        if samples is None:
            return self.line_wave, self.line_fft, self.status

        n        = len(samples)
        volts    = samples.astype(np.float64) * self.adc_to_v
        t_ms     = np.arange(n) * (1000.0 / self.sample_rate)

        self.line_wave.set_data(t_ms, volts)
        self.ax_wave.set_xlim(0, t_ms[-1])

        window   = np.hanning(n)
        fft_vals = np.fft.rfft(volts * window)
        fft_mag  = np.abs(fft_vals) * 2.0 / n
        fft_db   = 20 * np.log10(np.maximum(fft_mag, 1e-10))
        freqs    = np.fft.rfftfreq(n, d=1.0 / self.sample_rate)

        self.line_fft.set_data(freqs, fft_db)
        self.ax_fft.set_xlim(0, self.sample_rate / 2)

        vmin, vmax = volts.min(), volts.max()
        peak_idx   = np.argmax(fft_mag[1:]) + 1 if len(fft_mag) > 1 else 0
        peak_freq  = freqs[peak_idx] if peak_idx else 0
        peak_db    = fft_db[peak_idx] if peak_idx else -80

        self.status.set_text(
            f"Rate:{self.sample_rate} Hz  Samples:{n}\n"
            f"Vmin:{vmin:.3f}V  Vmax:{vmax:.3f}V  Vpp:{vmax-vmin:.3f}V\n"
            f"Peak:{peak_freq:.1f} Hz ({peak_db:.1f} dB)  "
            f"Frames:{self.reader.total_frames}  Dropped:{self.reader.dropped}"
        )
        return self.line_wave, self.line_fft, self.status

    def run(self):
        self.ani = animation.FuncAnimation(
            self.fig, self.update, interval=50, blit=True, cache_frame_data=False
        )
        plt.show()


def main():
    parser = argparse.ArgumentParser(description="STM32 USB Signal Analyzer")
    parser.add_argument("--port", help="Serial port (e.g., COM3, /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--rate", type=int, default=10000, help="Sample rate in Hz")
    args = parser.parse_args()

    port = args.port or find_stm32_port()
    if not port:
        print("Error: No STM32 device found. Use --port COM3 to specify manually.")
        sys.exit(1)

    print(f"Connecting to {port}...")
    try:
        ser = serial.Serial(port, args.baud, timeout=0.1)
    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)

    time.sleep(0.5)
    ser.write(f"rate {args.rate}\r\n".encode())
    time.sleep(0.1)
    ser.write(b"start\r\n")
    time.sleep(0.1)
    ser.read(ser.in_waiting or 0)   # drain text responses

    reader = FrameReader(ser)
    reader.start()
    print(f"Sampling at {args.rate} Hz. Close the plot window to stop.")

    scope = Oscilloscope(reader, args.rate)
    try:
        scope.run()
    except KeyboardInterrupt:
        pass
    finally:
        print("\nStopping...")
        ser.write(b"stop\r\n")
        time.sleep(0.1)
        reader.stop()
        ser.close()
        print("Done.")


if __name__ == "__main__":
    main()
