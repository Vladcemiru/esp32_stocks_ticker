# ESP32 Stocks Monitor

OLED SSD1306, Wi‑Fi, and HTTP APIs (Finnhub quotes + Fear & Greed). **Build with the ESP‑IDF extension in VS Code / Cursor** (or the IDF shell).

## Build and flash (ESP‑IDF)

1. Install **ESP‑IDF** and the **ESP‑IDF** extension (Espressif).
2. Open the project folder. The extension detects `CMakeLists.txt` and offers **Build**, **Flash**, **Monitor**.
3. From a terminal with the IDF environment loaded:
   ```bash
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
4. Set Wi‑Fi and API keys in `include/connectivity_config.h` (see `main/connectivity_config.h` for a minimal template). The main app includes `../include/connectivity_config.h` when present.

### Tickers (`main/tickers.txt`)

One ticker symbol per line (e.g. `AAPL`, `NVDA`). Empty lines and lines starting with `#` are ignored. The file is embedded at build time—after editing tickers, **rebuild and flash**. Maximum **32** tickers (RAM and API limits).

### Display mode (`main/view_config.h`)

Set **`STOCKS_DISPLAY_VIEW`** to one of:

- **`STOCKS_VIEW_SLIDES`** (0) — rotate slides (each stock + Fear & Greed).
- **`STOCKS_VIEW_HSCROLL`** (1) — title on top, horizontal scroll of all tickers with price and change, then a short F&G screen.
- **`STOCKS_VIEW_VSLOW`** (2) — slow vertical scroll of the full list + F&G line.

Tune timing with `VIEW_HSCROLL_*` and `VIEW_VSLOW_LINE_MS` in the same file.

**Flash:** After code changes, flash again if the board is connected. Common serial ports: `/dev/ttyUSB0`, `/dev/ttyACM0`, `/dev/ttyACM1`. Omit `-p` to let IDF auto-detect.

**“Error loading build.ninja”?** Delete the `build/` folder and build again (or `idf.py fullclean` then `idf.py build`). This often happens after switching between IDF and PlatformIO or changing CMake layout.

---

**PlatformIO:** An older Arduino/PlatformIO layout may still exist in the repo. If you only use ESP‑IDF, you can ignore or remove `platformio.ini`, `src/`, `arduino_ide_sketch/`, etc.

---

| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- |
