#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

// ==== ROLLBACK (NVS + particões) ====
#include <nvs.h>            // <<<
#include <nvs_flash.h>      // <<<
#include <esp_ota_ops.h>    // <<<

WebServer server(80);

// ===== Config WiFi AP =====
const char* AP_SSID = "OTA_VLB5";
const char* AP_PASS = "12345678"; // mínimo 8 chars

// ===== Auth básica =====
const char* USER = "Radcom";
const char* PASS = "radcom22";

// ---- Config validação/rollback ----
static const uint32_t OTA_VALIDATE_TIMEOUT_MS = 30000; // 30s  <<<

static bool   g_otaPending = false;    // estado lido do NVS       <<<
static uint32_t g_bootMs   = 0;        // timestamp de boot         <<<

// Página com campo opcional para MD5
static const char* PAGE_INDEX =
  "<!DOCTYPE html><html><head><meta charset='utf-8'/>"
  "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
  "<title>OTA Update</title></head><body>"
  "<h2>Atualizar Firmware (.bin)</h2>"
  "<form method='POST' action='/update' enctype='multipart/form-data'>"
  "<p><label>Arquivo: <input type='file' name='update' accept='.bin' required></label></p>"
  "<p><label>MD5 (opcional): <input name='md5' pattern='[a-fA-F0-9]{32}' "
  "placeholder='ex.: d41d8cd98f00b204e9800998ecf8427e'></label></p>"
  "<input type='submit' value='Enviar & Atualizar'>"
  "</form>"
  "<div id='log' style='margin-top:1rem;font-family:monospace;white-space:pre-wrap;'></div>"
  "<script>"
  "document.querySelector('form').addEventListener('submit',e=>{"
  "  const log=document.getElementById('log');"
  "  log.textContent='Enviando... Aguarde.\\n';"
  "});"
  "</script>"
  "</body></html>";

#ifndef FW_VERSION
#define FW_VERSION "v1.0.0"
#endif

// ===== util NVS (rollback) =====
static void nvsSetPending(bool pending) {                       // <<<
  nvs_handle h;
  if (nvs_open("ota", NVS_READWRITE, &h) == ESP_OK) {
    uint8_t v = pending ? 1 : 0;
    nvs_set_u8(h, "pending", v);
    nvs_commit(h);
    nvs_close(h);
  }
}

static bool nvsIsPending() {                                    // <<<
  nvs_handle h;
  uint8_t v = 0;
  if (nvs_open("ota", NVS_READONLY, &h) == ESP_OK) {
    nvs_get_u8(h, "pending", &v);
    nvs_close(h);
  }
  return v == 1;
}

static void rollbackToOtherPartition() {                        // <<<
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* next    = esp_ota_get_next_update_partition(NULL);
  // "next" é tipicamente a outra partição OTA (a anterior ao update).
  if (next) {
    esp_ota_set_boot_partition(next);
    Serial.println("[ROLLBACK] Definido boot para partição anterior. Reiniciando...");
    delay(100);
    esp_restart();
  } else {
    Serial.println("[ROLLBACK] Partição de fallback não encontrada!");
  }
}

// ===== Auth =====
bool checkAuth() {
  if (!server.authenticate(USER, PASS)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

void handleRoot() {
  if (!checkAuth()) return;
  server.send(200, "text/html", PAGE_INDEX);
}

void handleInfo() {
  if (!checkAuth()) return;
  String info;
  info.reserve(256);
  info += "{";
  info += "\"ip\":\""; info += WiFi.softAPIP().toString(); info += "\",";
  info += "\"fw_version\":\""; info += String(FW_VERSION); info += "\",";
  info += "\"ota_pending\":"; info += (g_otaPending ? "true" : "false");
  info += "}";
  server.send(200, "application/json", info);
}

void handleValidateNow() {                  // endpoint para marcar app como válido <<< 
  if (!checkAuth()) return;
  nvsSetPending(false);
  g_otaPending = false;
  server.send(200, "text/plain", "OK: firmware validado");
  Serial.println("[OTA] Firmware validado (pending=false)");
}

void handleUpdateUpload() {
  if (!checkAuth()) return;

  HTTPUpload& upload = server.upload();

  switch (upload.status) {
    case UPLOAD_FILE_START: {
      Serial.printf("[OTA] Iniciando upload: %s\n
