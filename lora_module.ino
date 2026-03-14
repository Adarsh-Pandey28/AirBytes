// ============================================================
//  LoRa Node — Heltec WiFi LoRa 32 V3 (ESP32-S3)
//  Text + GPS only | RadioLib (SX1262) + Adafruit SSD1306
//  NODE_ID: 0x01 = Node A, 0x02 = Node B
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// ── Change per node ────────────────────────────────────────
#define NODE_ID  0x02   // 0x01 = Node A, 0x02 = Node B
#define XOR_KEY  "SECRETKEY123"
#define KEY_LEN  12

// ── Vext power pin (powers built-in OLED) ─────────────────
#define VEXT_PIN  36    // LOW = ON, do NOT use as GPS pin

// ── Heltec V3 OLED pins ────────────────────────────────────
#define OLED_SDA  17
#define OLED_SCL  18
#define OLED_RST  21
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RST);

// ── Heltec V3 SX1262 LoRa pins ────────────────────────────
#define LORA_SCK   9
#define LORA_MISO  11
#define LORA_MOSI  10
#define LORA_CS    8
#define LORA_RST   12
#define LORA_DIO1  14
#define LORA_BUSY  13
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

// ── GPS UART1 (moved to GPIO 4, away from Vext pin 36) ────
#define GPS_RX  4
HardwareSerial gpsSerial(1);
TinyGPSPlus gps;

// ── Packet type ────────────────────────────────────────────
#define PKT_TEXT  0x01

// ── Message buffer ─────────────────────────────────────────
#define MAX_MSG  64
String inputMsg = "";

// ── Inbox ──────────────────────────────────────────────────
struct InboxMsg {
  uint8_t senderID;
  String  text;
  float   lat, lon;
  int     rssi;
};
InboxMsg inbox[3];
uint8_t inboxCount = 0;

// ── Buttons ────────────────────────────────────────────────
#define PIN_PRG    0
#define PIN_SPACE  45

#define DEBOUNCE_MS    220
#define LONG_PRESS_MS  600

unsigned long lastPressTime = 0;
unsigned long prgPressStart = 0;
bool          prgHeld       = false;

// ── RadioLib receive flag ──────────────────────────────────
volatile bool rxFlag = false;
void setRxFlag() { rxFlag = true; }

// ──────────────────────────────────────────────────────────
//  XOR CIPHER
// ──────────────────────────────────────────────────────────
void xorCipher(uint8_t* data, int len) {
  for (int i = 0; i < len; i++)
    data[i] ^= (uint8_t)XOR_KEY[i % KEY_LEN];
}

// ──────────────────────────────────────────────────────────
//  OLED HELPERS
// ──────────────────────────────────────────────────────────
void oledStatus(String l1, String l2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);  display.println(l1);
  display.setCursor(0, 14); display.println(l2);
  display.display();
}

void oledShowCompose() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(String("Node ") + (NODE_ID == 0x01 ? "A" : "B") + " | Compose");
  display.setCursor(0, 12); display.println("PRG short=BKSP");
  display.setCursor(0, 22); display.println("PRG long =SEND");
  display.setCursor(0, 32); display.println("SPACE btn=space");
  display.setCursor(0, 52);
  display.println(inputMsg.length() > 0 ? inputMsg.substring(0, 21) : "...");
  display.display();
}

void oledShowInbox(int slot) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  if (inboxCount == 0) {
    display.setCursor(0, 0); display.println("No messages yet");
    display.display(); return;
  }
  InboxMsg& m = inbox[slot % min((int)inboxCount, 3)];
  display.setCursor(0, 0);
  display.println(String("From: Node ") + (m.senderID == 0x01 ? "A" : "B"));
  display.setCursor(0, 10);
  display.println(String("RSSI: ") + m.rssi + " dBm");
  display.setCursor(0, 20);
  if (m.lat != 0.0f)
    display.println(String(m.lat, 4) + "," + String(m.lon, 4));
  else
    display.println("GPS: NO FIX");
  display.setCursor(0, 32);
  display.println(m.text.substring(0, 21));
  if (m.text.length() > 21) {
    display.setCursor(0, 42);
    display.println(m.text.substring(21, 42));
  }
  display.display();
}

// ──────────────────────────────────────────────────────────
//  SEND TEXT
// ──────────────────────────────────────────────────────────
void sendTextPacket() {
  if (inputMsg.length() == 0) {
    oledStatus("Nothing to send");
    delay(800); oledShowCompose(); return;
  }

  int32_t latInt = 0, lonInt = 0;
  if (gps.location.isValid()) {
    latInt = (int32_t)(gps.location.lat() * 1000000.0);
    lonInt = (int32_t)(gps.location.lng() * 1000000.0);
  }

  uint8_t msgLen = (uint8_t)min((int)inputMsg.length(), MAX_MSG);
  uint8_t encBuf[MAX_MSG];
  for (int i = 0; i < msgLen; i++) encBuf[i] = (uint8_t)inputMsg[i];
  xorCipher(encBuf, msgLen);

  uint8_t pkt[75]; int idx = 0;
  pkt[idx++] = PKT_TEXT;  pkt[idx++] = NODE_ID;
  pkt[idx++] = (latInt >> 24) & 0xFF; pkt[idx++] = (latInt >> 16) & 0xFF;
  pkt[idx++] = (latInt >>  8) & 0xFF; pkt[idx++] = latInt & 0xFF;
  pkt[idx++] = (lonInt >> 24) & 0xFF; pkt[idx++] = (lonInt >> 16) & 0xFF;
  pkt[idx++] = (lonInt >>  8) & 0xFF; pkt[idx++] = lonInt & 0xFF;
  pkt[idx++] = msgLen;
  memcpy(&pkt[idx], encBuf, msgLen); idx += msgLen;

  radio.transmit(pkt, idx);
  radio.startReceive();

  oledStatus("Sent!", inputMsg);
  inputMsg = "";
  delay(600);
  oledShowCompose();
}

// ──────────────────────────────────────────────────────────
//  HANDLE INCOMING PACKET
// ──────────────────────────────────────────────────────────
void handleIncomingPacket() {
  uint8_t buf[80];
  size_t  len = 0;
  int state = radio.readData(buf, len);
  if (state != RADIOLIB_ERR_NONE || len < 12) {
    radio.startReceive(); return;
  }

  uint8_t pktType  = buf[0];
  uint8_t senderID = buf[1];
  int     rssi     = (int)radio.getRSSI();

  int32_t latRaw = ((int32_t)buf[2] << 24) | ((int32_t)buf[3] << 16) |
                   ((int32_t)buf[4] <<  8) |  (int32_t)buf[5];
  int32_t lonRaw = ((int32_t)buf[6] << 24) | ((int32_t)buf[7] << 16) |
                   ((int32_t)buf[8] <<  8) |  (int32_t)buf[9];

  if (pktType == PKT_TEXT) {
    uint8_t msgLen = buf[10];
    if (msgLen == 0 || msgLen > 64) { radio.startReceive(); return; }
    uint8_t encBuf[64];
    memcpy(encBuf, &buf[11], msgLen);
    xorCipher(encBuf, msgLen);
    String decoded = "";
    for (int i = 0; i < msgLen; i++)
      if (encBuf[i] >= 32 && encBuf[i] <= 126) decoded += (char)encBuf[i];
    uint8_t slot = inboxCount % 3;
    inbox[slot] = {
      senderID, decoded,
      (latRaw != 0) ? (latRaw / 1000000.0f) : 0.0f,
      (lonRaw != 0) ? (lonRaw / 1000000.0f) : 0.0f,
      rssi
    };
    inboxCount++;
    oledShowInbox(slot);
  }

  radio.startReceive();
}

// ──────────────────────────────────────────────────────────
//  SETUP
// ──────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Enable Vext to power the built-in OLED — must be first
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100);

  // OLED init
  Wire.begin(OLED_SDA, OLED_SCL);
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);  delay(20);
  digitalWrite(OLED_RST, HIGH); delay(20);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay(); display.display();
  oledStatus("Booting...");

  // GPS on GPIO 4
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, -1);

  // LoRa SX1262
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  int state = radio.begin(865.0, 125.0, 9, 5, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 17);
  if (state != RADIOLIB_ERR_NONE) {
    oledStatus("LoRa FAILED!", String("Code: ") + state);
    while (1);
  }
  radio.setDio1Action(setRxFlag);
  radio.startReceive();

  // Buttons
  pinMode(PIN_PRG,   INPUT_PULLUP);
  pinMode(PIN_SPACE, INPUT_PULLUP);

  oledStatus(
    String("Node ") + (NODE_ID == 0x01 ? "A" : "B") + " Ready",
    "865MHz SF9 OK"
  );
  delay(1500);
  oledShowCompose();
}

// ──────────────────────────────────────────────────────────
//  LOOP
// ──────────────────────────────────────────────────────────
void loop() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());

  if (rxFlag) { rxFlag = false; handleIncomingPacket(); }

  unsigned long now = millis();

  // PRG: short press = backspace, long press = send
  if (digitalRead(PIN_PRG) == LOW) {
    if (!prgHeld) { prgHeld = true; prgPressStart = now; }
    else if (now - prgPressStart >= LONG_PRESS_MS && prgPressStart != 0) {
      prgPressStart = 0;
      sendTextPacket();
    }
  } else {
    if (prgHeld) {
      prgHeld = false;
      unsigned long held = now - prgPressStart;
      if (held > 20 && held < LONG_PRESS_MS) {
        if (inputMsg.length() > 0) {
          inputMsg.remove(inputMsg.length() - 1);
          oledShowCompose();
        }
      }
    }
  }

  // SPACE button = add a space character
  if (digitalRead(PIN_SPACE) == LOW && (now - lastPressTime > DEBOUNCE_MS)) {
    lastPressTime = now;
    if (inputMsg.length() < MAX_MSG) {
      inputMsg += ' ';
      oledShowCompose();
    }
  }
}