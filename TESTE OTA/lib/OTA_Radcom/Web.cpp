#include "Web.h"
#include "OTA.h"
#include <WiFi.h>

TimerHandle_t xTimerWifi = nullptr;

static const char *EXPECTED_AUTH = "Basic UmFkY29tOnJhZGNvbTIy";
/*USUARIO: Radcom
  SENHA:   radcom22 
  */

// 'server' precisa existir em algum lugar (defina em main.cpp)
extern AsyncWebServer server;

Web::Web(String fwVersion)
: FW_VERSION(fwVersion)// inicializa o membro
{
  // opcional: log
  // Serial.println("[Web] FW_VERSION = " + FW_VERSION);
}

// Autenticação nativa do AsyncWebServer (evita EXPECTED_AUTH)
bool Web::autentication(AsyncWebServerRequest *req)
{
  if (!authEnabled) return true;

  if (req->hasHeader("Authorization"))
  {
    AsyncWebHeader *h = req->getHeader("Authorization");
    if (h && h->value() == EXPECTED_AUTH)
      return true;
  }
  // Sem/errada -> 401 com prompt
  AsyncWebServerResponse *resp = req->beginResponse(401, "text/plain", "Unauthorized");
  resp->addHeader("WWW-Authenticate", "Basic realm=\"OTA\"");
  req->send(resp);
  return false;
}

void Web::pageOTA()
{
  server.on("/ota", HTTP_GET, [this](AsyncWebServerRequest *req)
            {
    if (!autentication(req)) return;

    static const char PAGE_OTA[] PROGMEM = R"HTML(
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
      const r = await fetch('/ota/update', { method:'POST', body:fd });
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

   
    req->send_P(200, "text/html", PAGE_OTA); });
}

// OBS: seu HTML chama fetch('/info'), então exponha '/info' (não '/ota/info')
void Web::pageInfo()
{
  server.on("/info", HTTP_GET, [this](AsyncWebServerRequest *req)
            {
    if (!autentication(req)) return;
    String json;
    json.reserve(128);
    json += "{";
    json += "\"ip\":\""; json += WiFi.softAPIP().toString(); json += "\",";
    json += "\"fw_version\":\""; json += FW_VERSION; json += "\"";
    json += "}";
    req->send(200, "application/json", json);
});
}

void Web::pageUpdate()
{
  server.on("/ota/update", HTTP_POST, [](AsyncWebServerRequest *req)
            { OTA::finishUpdate(req); }, [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final)
            { OTA::handleUpload(req, filename, index, data, len, final); });
}

void Web::WEB_OTA(bool autenticacao)
{
  authEnabled = autenticacao;
  pageOTA();
  pageInfo();
  pageUpdate();
}
