#include "esp_camera.h"
#include <I2S.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <map>
// #include <ei-project_inferencing.h>   // TODO: Edge Impulse

const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL    = "http://192.168.1.100:5000/photo";

// Camera pins (XIAO ESP32S3 Sense)
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39
#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

// Microphone pins
#define I2S_WS  42
#define I2S_SD  41
#define I2S_SCK -1

#define SAMPLE_RATE    16000
#define SAMPLE_BITS    16
#define MIC_BUFFER_LEN 512

enum Filter {
  FILTER_NONE       = 0,
  FILTER_VINTAGE    = 1,
  FILTER_BLACK_WHITE = 2,
  FILTER_COLOR_POP  = 3,
  FILTER_FACE_FRAME = 4
};

const char* filterNames[] = {
  "none", "vintage", "blackWhite", "colorPop", "faceFrame"
};

Filter currentFilter = FILTER_NONE;
bool   takePhoto     = false;
int16_t audioBuffer[MIC_BUFFER_LEN];

// ── Setup ────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  initCamera();
  initMicrophone();
  connectWifi();
  Serial.println("[OK] Ready.");
}

// TODO: Remove after testing
const std::map<char, Filter> keyToFilter = {
  { 'f', FILTER_NONE        },
  { 'v', FILTER_VINTAGE     },
  { 'b', FILTER_BLACK_WHITE },
  { 'c', FILTER_COLOR_POP   },
  { 's', FILTER_FACE_FRAME  },
};

// TODO: Activate edge impulse
const std::map<String, Filter> labelToFilter = {
  { "photo",           FILTER_NONE        },
  { "vintage",         FILTER_VINTAGE     },
  { "black and white", FILTER_BLACK_WHITE },
  { "color pop",       FILTER_COLOR_POP   },
  { "face frame",      FILTER_FACE_FRAME  },
};

// ── Loop ─────────────────────────────────────────────────────

void loop() {
  // --- testing
  if (Serial.available()) {
    char c = Serial.read();
    auto it = keyToFilter.find(c);
    if (it != keyToFilter.end()) {
      takePhoto     = true;
      currentFilter = it->second;
    }
  }

  // --- EDGE IMPULSE ---
  // int bytesRead = I2S.readBytes((char*)audioBuffer, sizeof(audioBuffer));
  // if (bytesRead > 0) {
  //   signal_t signal;
  //   numpy::signal_from_buffer(audioBuffer, bytesRead / 2, &signal);
  //   ei_impulse_result_t result;
  //   run_classifier(&signal, &result, false);
  //
  //   String topLabel = "";
  //   float  topScore = 0.7;   // confidence threshold
  //   for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
  //     if (result.classification[i].value > topScore) {
  //       topScore = result.classification[i].value;
  //       topLabel = result.classification[i].label;
  //     }
  //   }
  //   auto it = labelToFilter.find(topLabel);
  //   if (it != labelToFilter.end()) {
  //     takePhoto     = true;
  //     currentFilter = it->second;
  //   }
  // }

  if (takePhoto) {
    takePhoto = false;
    camera_fb_t* fb = captureFrame();
    if (fb) {
      sendPhoto(fb, currentFilter);
      esp_camera_fb_return(fb);
    }
  }

  delay(10);
}

// ── Camera ───────────────────────────────────────────────────

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init failed: 0x%x — check Tools > PSRAM > OPI PSRAM\n", err);
    while (true) delay(1000);
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 0);
  s->set_hmirror(s, 0);
}

camera_fb_t* captureFrame() {
  camera_fb_t* fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);       // Discard first frame (often overexposed)
  fb = esp_camera_fb_get();
  if (!fb) Serial.println("[CAM] Frame capture failed");
  return fb;
}

// ── Microphone ───────────────────────────────────────────────

void initMicrophone() {
  I2S.setAllPins(I2S_SCK, I2S_WS, I2S_SD, -1, -1);
  if (!I2S.begin(PDM_MONO_MODE, SAMPLE_RATE, SAMPLE_BITS)) {
    Serial.println("[MIC] Init failed — check Sense module connection");
    while (true) delay(1000);
  }
}

// ── WiFi & HTTP ──────────────────────────────────────────────

void connectWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts++ < 20) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("\n[WIFI] Connected — %s\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("\n[WIFI] Could not connect — continuing without WiFi");
}

void sendPhoto(camera_fb_t* fb, Filter filter) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] No WiFi — photo not sent");
    return;
  }

  HTTPClient http;
  String url = String(SERVER_URL) + "?filter=" + filterNames[filter];
  http.begin(url);
  http.addHeader("Content-Type", "image/jpeg");

  int code = http.POST(fb->buf, fb->len);
  if (code == HTTP_CODE_OK)
    Serial.printf("[HTTP] OK — filter: %s\n", filterNames[filter]);
  else
    Serial.printf("[HTTP] Error %d\n", code);

  http.end();
}
