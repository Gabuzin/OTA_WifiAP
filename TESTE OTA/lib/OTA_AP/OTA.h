#ifndef OTA_H
#define OTA_H

#include <Arduino.h>
#include <WiFi.h>
#include <Update.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

class OTA {
private:
  static void log_reset_reason();
  static void log_ota_state();
  void early_bootloop_guard();
  void Get_partition_Valid();
//void TimerCreate(unsigned long)
public:
  // ===== WiFi AP =====
  const char* AP_SSID;
  const char* AP_PASS;

  // ===== Credenciais (Basic Auth manual) =====
  const char* USER;
  const char* PASS;

  unsigned long timer;

  // ---- Construtor com atributos ----
  OTA(const char* ssid,
      const char* senha,
      unsigned long timerS);

  // Sobe o AP usando os atributos
  void WifiAP(unsigned long timer);

  // Info OTA
  String Get_running_partition();
  String Get_nextBoot_partition();
  String Get_nextUpdate_partition();
  
  void bootGuard();

  void zerar_contador_boot();
  // Handlers OTA (estáticos p/ usar como callbacks)
  static void finishUpdate(AsyncWebServerRequest* request);
  static void handleUpload(AsyncWebServerRequest* request, String filename,
                           size_t index, uint8_t* data, size_t len, bool final);
};

extern TimerHandle_t xTimerWifi;
extern TimerHandle_t xTimerVerify;
extern TaskHandle_t xBootVerify;
// Declaração (apenas) da instância global
extern OTA ota_object;

#endif // OTA_H
