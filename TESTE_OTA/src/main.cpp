#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// ===== Config WiFi AP =====
const char* AP_SSID = "OTA_VLB5";
const char* AP_PASS = "radcom22"; // mínimo 8 chars

// ===== Auth básica =====
const char* USER = "Radcom";
const char* PASS = "radcom22";

#ifndef FW_VERSION
#define FW_VERSION "v1.2.0"
#endif

Preferences prefs;



WebServer server(80);

// ===== Página inicial OTA =====
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Atualização OTA - ESP32</title>
  <style>
    body {
      background-color: #f2f2f2;
      font-family: Arial, sans-serif;
      text-align: center;
      margin-top: 60px;
    }
    h1 { color: #333; }
    .version {
      background: #fff;
      border: 1px solid #ddd;
      display: inline-block;
      padding: 10px 20px;
      margin-bottom: 20px;
      border-radius: 8px;
    }
    form {
      background: #fff;
      border: 1px solid #ddd;
      display: inline-block;
      padding: 20px;
      border-radius: 8px;
    }
    input[type=file] {
      padding: 8px;
      margin-bottom: 10px;
    }
    input[type=submit] {
      background-color: #4CAF50;
      color: white;
      border: none;
      padding: 10px 20px;
      border-radius: 6px;
      cursor: pointer;
      font-size: 15px;
    }
    input[type=submit]:hover {
      background-color: #45a049;
    }
  </style>
</head>
<body>
  <h1>Atualização OTA - ESP32</h1>
  <div class="version">Versão atual: <b>%VERSION%</b></div>
  <form method='POST' action='/update' enctype='multipart/form-data'>
    <input type='file' name='firmware'>
    <br><br>
    <input type='submit' value='Enviar e Atualizar'>
  </form>
  <p style="margin-top:20px; color:#666;">SSID do AP: <b>%SSID%</b></p>
</body>
</html>
)rawliteral";

// ====== Funções ======

bool checkAuth() {
  if (!server.authenticate(USER, PASS)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

String Get_running_partition() {
  const esp_partition_t* p = esp_ota_get_running_partition();
  return p ? String(p->label) : String("unknown");   // "factory", "ota_0", "ota_1"
}

String Get_nextBoot_partition() {
  const esp_partition_t* p = esp_ota_get_boot_partition();  
  return p ? String(p->label) : String("unknown");
}
String Get_nextUpdate_partition() {
  const esp_partition_t* p = esp_ota_get_next_update_partition(NULL);  
  return p ? String(p->label) : String("unknown");
}

void handleRoot() {
  if (!checkAuth()) return;

  String html = htmlPage;
  html.replace("%VERSION%", FW_VERSION);
  html.replace("%SSID%", AP_SSID);
  server.send(200, "text/html", html);
}

void handleUpdate() {
  if (!checkAuth()) return;

  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    prefs.begin("ota",false);
    prefs.putString("last_ok", Get_running_partition());
    prefs.putString("next_boot")
    
    Serial.printf("Iniciando update: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Atualização completa: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
  yield();
}

void handleUpdateDone() {
  if (!checkAuth()) return;
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", Update.hasError() ? "Falha na atualização" : "Atualização concluída, reiniciando...");
  delay(1500);
  ESP.restart();
}


void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Modo OTA AP Iniciado ===");
  Serial.printf("Versão: %s\n", FW_VERSION);



  // Inicia AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Acesse via navegador: http://");
  Serial.println(IP);

  // Rotas
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdate);
  server.begin();

  Serial.println("Servidor HTTP iniciado.");
}

void loop() {
  server.handleClient();
}
