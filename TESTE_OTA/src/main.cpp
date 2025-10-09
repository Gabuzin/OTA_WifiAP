#include <Arduino.h>
#include <WiFi.h>
#include <Update.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

// ===== WiFi AP =====
const char* AP_SSID = "OTA_VLB5";
const char* AP_PASS = "radcom22";

// ===== Credenciais (Basic Auth manual) =====
const char* USER = "Radcom";
const char* PASS = "radcom22";

// "Basic " + base64("USER:PASS")
// Radcom:radcom22 -> UmFkY29tOnJhZGNvbTIy
static const char* EXPECTED_AUTH = "Basic UmFkY29tOnJhZGNvbTIy";

// ===== Versão de firmware =====
#ifndef FW_VERSION
#define FW_VERSION "v1.0.0"
#endif

// ===== HTML (igual ao seu, simples/estético) =====
static const char PAGE_INDEX[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="pt-br">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>RadCom - OTA</title>
  <style>
    #cssmenu, #cssmenu ul, #cssmenu li, #cssmenu img {
      margin:0; padding:0; list-style:none; text-decoration:none; line-height:1; text-align:center;
    }
    #cssmenu { width:100%; background:#FFFF00; text-align:left; }
    #cssmenu>ul{ margin:0; padding:0; }
    #cssmenu>ul>li{ display:inline-block; border-bottom:1px solid #fff; }
    #cssmenu img{ display:block; height:85px; }
    body { margin:0; background:#25598A; font-family: Arial, Helvetica, sans-serif; color:#000; }
    .form-container { max-width:480px; margin:20px auto; background:#fff; border-radius:4px;
      padding:20px; box-shadow:0 2px 6px rgba(0,0,0,.1); color:#000; display:flex; flex-direction:column; gap:18px; }
    .panel { width:95%; background:#ececec; border:1px solid #000; border-radius:4px;
      box-shadow:0 2px 6px rgba(0,0,0,.3); padding:12px; margin:0 auto; display:flex; flex-direction:column; gap:10px; }
    .row { display:flex; justify-content:space-between; align-items:center; }
    .label { font-weight:bold; }
    .value { font-family:monospace; }
    .field { display:block; margin:6px 0 4px; font-weight:bold; }
    input[type="file"] { width:96%; padding:8px; border:1px solid #ccc; border-radius:4px; background:#fff; color:#000; }
    .btn { display:inline-block; text-align:center; cursor:pointer; user-select:none;
      border:1px solid #000; border-radius:5px; height:44px; line-height:44px;
      padding:0 16px; font-weight:bold; color:#fff; transition:filter .2s ease; }
    .btn:disabled { opacity:.6; cursor:not-allowed; }
    .btn.primary { background:#4CAF50; }
    .btn.primary:hover { filter:brightness(0.95); }
    .actions { display:flex; gap:10px; justify-content:center; flex-wrap:wrap; }
    #log { margin-top:8px; font-family:monospace; white-space:pre-wrap; background:#f7f7f7;
      border:1px dashed #aaa; padding:8px; border-radius:4px; }
    @media (max-width:600px) {
      .form-container { width:90%; padding:16px; }
      input[type="file"] { width:100%; }
    }
  </style>
</head>
<body>
  <div id="cssmenu">
    <ul><li class="active">
      <img src="data:image/gif;base64,R0lGODlhAQhVAIAAAP///wAAACH5BAAAAAAALAAAAAABCFUAAAIChI+py+0Po5yUFQA7" alt="Logo">
    </li></ul>
  </div>

  <div class="form-container">
    <h2 style="margin:0; text-align:center;">Atualização de Firmware (OTA)</h2>
    <div class="panel">
      <div class="row"><span class="label">Firmware:</span><span class="value" id="fw">--</span></div>
      <div class="row"><span class="label">IP do AP:</span><span class="value" id="ip">--</span></div>
    </div>

    <div class="panel">
      <label class="field">Arquivo (.bin)</label>
      <input id="bin" type="file" name="update" accept=".bin" required>
      <div class="actions">
        <button id="btnSend" class="btn primary">Enviar & Atualizar</button>
      </div>
      <div id="log">Pronto.</div>
      <div style="color:#333;font-size:.9em;">Após atualizar, o dispositivo reinicia automaticamente.</div>
    </div>
  </div>

  <script>
  async function refreshInfo(){
    try{
      const r = await fetch('/info', {cache:'no-store'});
      if(!r.ok) throw new Error(r.status);
      const j = await r.json();
      document.getElementById('fw').textContent = j.fw_version ?? '--';
      document.getElementById('ip').textContent = j.ip ?? '--';
    }catch(e){ console.error(e); }
  }

  async function sendUpdate(){
    const fileInput = document.getElementById('bin');
    const btnSend   = document.getElementById('btnSend');
    const log       = document.getElementById('log');

    if(!fileInput.files || fileInput.files.length===0){
      alert('Selecione um arquivo .bin');
      return;
    }
    const fd = new FormData();
    fd.append('update', fileInput.files[0]);

    btnSend.disabled = true;
    log.textContent = 'Enviando... Aguarde.\n';
    try{
      const r = await fetch('/update', { method:'POST', body:fd });
      const txt = await r.text();
      log.textContent += 'Servidor respondeu: ' + txt + '\n';
      if(r.ok) log.textContent += 'Se o update foi aceito, o dispositivo vai reiniciar...\n';
      else     log.textContent += 'Falha no update (HTTP ' + r.status + ').\n';
    }catch(e){
      log.textContent += 'Erro no envio: ' + e + '\n';
    }finally{
      btnSend.disabled = false;
      setTimeout(refreshInfo, 4000);
    }
  }

  document.addEventListener('DOMContentLoaded', ()=>{
    refreshInfo();
    setInterval(refreshInfo, 3000);
    document.getElementById('btnSend').addEventListener('click', sendUpdate);
  });
  </script>
</body>
</html>
)HTML";

// ===== Basic Auth manual (sem MD5) =====
static bool checkAuth(AsyncWebServerRequest* request) {
  if (request->hasHeader("Authorization")) {
    AsyncWebHeader* h = request->getHeader("Authorization");
    if (h && h->value() == EXPECTED_AUTH) return true;
  }
  // Sem/errada -> 401 com prompt
  AsyncWebServerResponse* resp = request->beginResponse(401, "text/plain", "Unauthorized");
  resp->addHeader("WWW-Authenticate", "Basic realm=\"OTA\"");
  request->send(resp);
  return false;
}

// ===== Handlers =====
static void handleRoot(AsyncWebServerRequest* request) {
  if (!checkAuth(request)) return;
  request->send_P(200, "text/html", PAGE_INDEX);
}

static void handleInfo(AsyncWebServerRequest* request) {
  if (!checkAuth(request)) return;
  String json;
  json.reserve(128);
  json += "{";
  json += "\"ip\":\""; json += WiFi.softAPIP().toString(); json += "\",";
  json += "\"fw_version\":\""; json += FW_VERSION; json += "\"";
  json += "}";
  request->send(200, "application/json", json);
}

// Upload OTA (sem MD5)
static void handleUpload(AsyncWebServerRequest* request, String filename, size_t index,
                         uint8_t* data, size_t len, bool final) {
  if (!checkAuth(request)) return;

  if (index == 0) {
    Serial.printf("[OTA] Iniciando upload: %s\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  }
  if (len) {
    if (Update.write(data, len) != len) {
      Update.printError(Serial);
    }
  }
  if (final) {
    if (Update.end(true)) {
      Serial.printf("[OTA] Sucesso: %u bytes\n", (unsigned)index + len);
    } else {
      Update.printError(Serial);
    }
  }
}

static void finishUpdate(AsyncWebServerRequest* request) {
  if (!checkAuth(request)) return;

  if (!Update.hasError()) {
    request->send(200, "text/plain", "Atualização concluída! Reiniciando...");
    Serial.println("[OTA] Reiniciando...");
    delay(1500);
    ESP.restart();
  } else {
    request->send(500, "text/plain", "Falhou! Veja o log serial.");
  }
}

// ===== Setup / Loop =====
void setup() {
  Serial.begin(115200);
  delay(300);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println();
  Serial.print("AP SSID: "); Serial.println(AP_SSID);
  Serial.print("AP IP  : "); Serial.println(WiFi.softAPIP());

  // Rotas
  server.on("/ota", HTTP_GET, handleRoot);
  server.on("/ota/info", HTTP_GET, handleInfo);

  // Upload multipart (POST /update)
  server.on("/ota/update", HTTP_POST, finishUpdate,
            handleUpload); // onFileUpload

  server.begin();
  Serial.println("Servidor OTA (Async) pronto!");
}

void loop() {
  // nada: AsyncWebServer não precisa de handleClient()
}
