#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Seeed_Arduino_SSCMA.h>

const char* WIFI_SSID     = "UPV-PSK";
const char* WIFI_PASSWORD = "Pr4ct1c4s-UPV!";

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

WebServer server(80);
SSCMA AI;

String last_detected_gesture = "none";
int max_confidence = 0;

void setup() {
  Serial.begin(115200);
  initCamera();
  connectWifi();

  if (!AI.begin()) {
    Serial.println("[AI] Error");
  }

  server.on("/photo", HTTP_GET, handlePhoto);
  server.begin();
}

void loop() {
  server.handleClient();

  if (AI.invoke() == 0) { 
    String g_top = "none";
    int s_top = 60;

    for (auto& box : AI.boxes()) {
      if (box.score > s_top) {
        s_top = box.score;
        if (box.target == 0 || String(box.label) == "rock") g_top = "rock";
        else if (box.target == 1 || String(box.label) == "paper") g_top = "paper";
        else if (box.target == 2 || String(box.label) == "scissors") g_top = "scissors";
      }
    }
    last_detected_gesture = g_top;
    max_confidence = s_top;
  }
  delay(10);
}

void handlePhoto() {
  String filter = server.arg("filter");
  if (filter == "") filter = "none";

  camera_fb_t* fb = captureFrame();
  if (!fb) {
    server.send(500, "text/plain", "Error");
    return;
  }

  server.sendHeader("X-Filter", filter);
  server.sendHeader("X-Gesto", last_detected_gesture);
  server.sendHeader("X-Confianza", String(max_confidence));

  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

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
  config.grab_mode    = CAMERA_GRAB_LATEST;
  config.fb_location  = CAMERA_FB_IN_PSRAM;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count     = 2;
  } else {
    config.frame_size   = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    while (true) delay(1000);
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 0);
  s->set_hmirror(s, 0);
}

camera_fb_t* captureFrame() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    return nullptr;
  }
  return fb;
}

void connectWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts++ < 20) {
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED) {
    while (true) delay(1000);
  }
}
