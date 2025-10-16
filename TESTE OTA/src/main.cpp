#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include <OTA.h>
#include <Web.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

Web web("v0.0.4"); // version
OTA ota_object("WifiName", "password", 600);// Wifi Name, password, timer to turn off wifi(if null -> always on) 
AsyncWebServer server(80);





void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(50));

  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL); // watchdog 

  ota_object.bootGuard(); // boot verification (functions in portuguese(BR))

  ota_object.WifiAP(ota_object.timer);
  vTaskDelay(pdMS_TO_TICKS(50));
  web.WEB_OTA(true); // true: web autentication, false: no autentication -> static const char *EXPECTED_AUTH = "Basic Hash"(web.cpp)
  vTaskDelay(pdMS_TO_TICKS(50));
  server.begin();

  // ota_object.Get_partition_Valid();
}
void loop() {
  esp_task_wdt_reset();
  vTaskDelay(pdMS_TO_TICKS(100));
}