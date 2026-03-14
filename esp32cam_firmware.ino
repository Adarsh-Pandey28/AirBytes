// ============================================================
//  ESP32-CAM Firmware — AI-Thinker board with OV2640
//  This runs independently on the ESP32-CAM.
//  It waits for "CAPTURE\n" from Heltec over UART0,
//  then streams: [4-byte size big-endian] + [JPEG bytes]
// ============================================================

#include "esp_camera.h"

// AI-Thinker ESP32-CAM pin definitions
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

void setup() {
  // UART0 communicates with Heltec
  Serial.begin(115200);
  Serial.setTimeout(100);

  camera_config_t config;
  config.ledc_channel  = LEDC_CHANNEL_0;
  config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0        = Y2_GPIO_NUM;
  config.pin_d1        = Y3_GPIO_NUM;
  config.pin_d2        = Y4_GPIO_NUM;
  config.pin_d3        = Y5_GPIO_NUM;
  config.pin_d4        = Y6_GPIO_NUM;
  config.pin_d5        = Y7_GPIO_NUM;
  config.pin_d6        = Y8_GPIO_NUM;
  config.pin_d7        = Y9_GPIO_NUM;
  config.pin_xclk      = XCLK_GPIO_NUM;
  config.pin_pclk      = PCLK_GPIO_NUM;
  config.pin_vsync     = VSYNC_GPIO_NUM;
  config.pin_href      = HREF_GPIO_NUM;
  config.pin_sscb_sda  = SIOD_GPIO_NUM;
  config.pin_sscb_scl  = SIOC_GPIO_NUM;
  config.pin_pwdn      = PWDN_GPIO_NUM;
  config.pin_reset     = RESET_GPIO_NUM;
  config.xclk_freq_hz  = 20000000;
  config.pixel_format  = PIXFORMAT_JPEG;
  config.frame_size    = FRAMESIZE_QVGA;   // 320x240
  config.jpeg_quality  = 12;               // 0-63, lower = better quality
  config.fb_count      = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    // Flash LED to signal failure
    Serial.println("CAM_INIT_FAIL");
    return;
  }

  // Allow sensor to stabilise
  delay(500);
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "CAPTURE") {
      camera_fb_t* fb = esp_camera_fb_get();
      if (!fb) {
        // Send zero-size to signal failure
        uint8_t fail[4] = {0,0,0,0};
        Serial.write(fail, 4);
        return;
      }

      // Send 4-byte image size (big-endian)
      uint32_t sz = fb->len;
      uint8_t szBuf[4];
      szBuf[0] = (sz >> 24) & 0xFF;
      szBuf[1] = (sz >> 16) & 0xFF;
      szBuf[2] = (sz >>  8) & 0xFF;
      szBuf[3] =  sz        & 0xFF;
      Serial.write(szBuf, 4);

      // Stream JPEG bytes
      Serial.write(fb->buf, fb->len);
      Serial.flush();

      esp_camera_fb_return(fb);
    }
  }
}
