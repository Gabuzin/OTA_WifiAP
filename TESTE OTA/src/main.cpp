#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include <OTA.h>
#include <Web.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

Web web("v0.0.4");
OTA ota_object("OTA-VLB5", "radcom22", 600);
AsyncWebServer server(80);

// extern void early_bootloop_guard();
// extern void zerar_contador_boot();



void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(50));

  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL);

  ota_object.bootGuard();

  ota_object.WifiAP(ota_object.timer);
  vTaskDelay(pdMS_TO_TICKS(50));
  web.WEB_OTA(true);
  vTaskDelay(pdMS_TO_TICKS(50));
  server.begin();

  // ota_object.Get_partition_Valid();
}
void loop() {
  esp_task_wdt_reset();
  vTaskDelay(pdMS_TO_TICKS(100));
}