# 📡 Air Bytes
> Offline, encrypted, GPS-tagged peer-to-peer messaging over LoRa — no internet required.

Built for the **Heltec WiFi LoRa 32 V3** (ESP32-S3) using RadioLib and the onboard SSD1306 OLED.

---

## Features

- 🔒 XOR-encrypted text messages
- 📍 GPS coordinates attached to every message
- 📟 Live OLED display with inbox + compose UI
- 📶 LoRa @ 865 MHz, SF9 (India ISM band)
- 🔋 Two-button operation — no touchscreen needed

---

## Hardware Required

| Component | Details |
|---|---|
| Heltec WiFi LoRa 32 V3 | × 2 (one per node) |
| GPS Module | NEO-6M or NEO-8M |
| Tactile Button | External SPACE button |

---

## Wiring

| GPS Module | Heltec V3 Pin |
|---|---|
| TX | GPIO 4 |
| VCC | 3.3V |
| GND | GND |

| SPACE Button | Heltec V3 Pin |
|---|---|
| Leg 1 | GPIO 45 |
| Leg 2 | GND |

> Everything else (OLED, LoRa, PRG button) is already onboard.

---

## Button Controls

| Button | Action |
|---|---|
| PRG short press | Backspace |
| PRG long press | Send message |
| SPACE press | Add a space |

---

## Setup (Arduino IDE)

**1. Add ESP32 board support**
> File → Preferences → paste this URL:
> `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
> Then: Tools → Boards Manager → install **esp32 by Espressif Systems**

**2. Install libraries** via Tools → Manage Libraries:
- `RadioLib` — Jan Gromeš
- `Adafruit SSD1306` — Adafruit
- `Adafruit GFX Library` — Adafruit
- `TinyGPS++` — Mikal Hart

**3. Select board**
> Tools → Board → **Heltec WiFi LoRa 32(V3)**

**4. Set Node ID** in the sketch before uploading:
```cpp
#define NODE_ID  0x01   // Node A
#define NODE_ID  0x02   // Node B
```

**5. Upload** — hold BOOT button if it fails to connect.

---

## How It Works

```
[Node A] → type message → PRG long press
        → XOR encrypt → pack with GPS coords
        → transmit over LoRa @ 865 MHz

[Node B] → receives packet → decrypt → show on OLED
        → displays sender, RSSI, GPS, message text
```

Inbox holds the last **3 messages**. Older messages are overwritten in a circular buffer.

---

## Configuration

| Setting | Location | Default |
|---|---|---|
| Node ID | `#define NODE_ID` | `0x01` |
| Encryption key | `#define XOR_KEY` | `"SECRETKEY123"` |
| LoRa frequency | `radio.begin(...)` | `865.0 MHz` |
| Spreading factor | `radio.begin(...)` | `SF9` |

>  Both nodes must use the **same** `XOR_KEY` to communicate.

---


