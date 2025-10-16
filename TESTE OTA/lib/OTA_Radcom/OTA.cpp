#include "OTA.h"
#include "Web.h"
#include <Preferences.h>
#include <FS.h>
#include <SPIFFS.h> // ou <LittleFS.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_partition.h"
#include "esp_flash.h"

Preferences prefs;
TimerHandle_t xTimerVerify = nullptr;
TaskHandle_t  xBootVerify  = nullptr;
// INFORMAÇÃO DE TAMANHO DAS PARTIÇÕES OCUPADAS
void printFlashChipInfo()
{
  uint32_t size = 0;
  esp_flash_get_size(NULL, &size);
  Serial.printf("[FLASH] Tamanho físico: %.1f MB\n", size / (1024.0 * 1024.0));
}

void printOtaPartitions()
{
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);

  Serial.printf("[OTA] Rodando em: %s | Tamanho: %u bytes\n",
                running->label, running->size);
  Serial.printf("[OTA] Próxima partição: %s | Tamanho: %u bytes\n",
                next->label, next->size);
}
void printFSUsage()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS mount failed");
    return;
  }

  size_t total = SPIFFS.totalBytes();
  size_t used = SPIFFS.usedBytes();

  Serial.printf("[SPIFFS] Total: %u bytes | Usado: %u bytes (%.1f%%)\n",
                (unsigned)total, (unsigned)used, (100.0 * used / total));

  SPIFFS.end();
}

// conta numero de boot para proteção e volta para programa antigo se nao verificar que esta ok

void OTA::log_reset_reason()
{
  esp_reset_reason_t r = esp_reset_reason();
  const char *txt = "UNKNOWN";
  switch (r)
  {
  case ESP_RST_POWERON:
    txt = "POWERON";
    break;
  case ESP_RST_EXT:
    txt = "EXT";
    break;
  case ESP_RST_SW:
    txt = "SW";
    break;
  case ESP_RST_PANIC:
    txt = "PANIC";
    break;
  case ESP_RST_INT_WDT:
    txt = "INT_WDT";
    break;
  case ESP_RST_TASK_WDT:
    txt = "TASK_WDT";
    break;
  case ESP_RST_WDT:
    txt = "WDT";
    break;
  case ESP_RST_DEEPSLEEP:
    txt = "DEEPSLEEP";
    break;
  case ESP_RST_BROWNOUT:
    txt = "BROWNOUT";
    break;
  case ESP_RST_SDIO:
    txt = "SDIO";
    break;
  default:
    break;
  }
  Serial.printf("[BOOT] Reset reason: %d (%s)\n", (int)r, txt);
}

void OTA::early_bootloop_guard()
{
  prefs.begin("boot", false);
  uint32_t cnt = prefs.getUInt("cnt", 0) + 1;
  prefs.putUInt("cnt", cnt);
  prefs.end();

  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t st;
  esp_ota_get_state_partition(running, &st);

  if (st == ESP_OTA_IMG_PENDING_VERIFY && cnt >= 3)
  {
    Serial.println("[OTA] Boot-loop detectado → rollback agora");
    esp_ota_mark_app_invalid_rollback_and_reboot(); // não retorna
  }
}

void OTA::zerar_contador_boot()
{
  prefs.begin("boot", false);
  prefs.putUInt("cnt", 0);
  prefs.end();
}



void OTA::log_ota_state()
{
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *next_p = esp_ota_get_next_update_partition(NULL);
  esp_ota_img_states_t st = (esp_ota_img_states_t)0;
  esp_err_t err = esp_ota_get_state_partition(running, &st);

  if (!running)
  {
    Serial.println("[OTA] running = NULL (nao foi possivel obter a particao atual)");
    return;
  }
  else
  {
    Serial.printf("[OTA] Running partition: %s | Size partition: %u \n", running ? running->label : "unknown", running->size);

    if (running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY)
    {
      Serial.println("[OTA] App = FACTORY → sem estado OTA para consultar.");
    }
    else
    {
      if (esp_ota_get_state_partition(running, &st) == ESP_OK)
      {

        Serial.printf("[OTA] Image state: %d (0=INVALID,1=VALID,2=PENDING_VERIFY,3=ABORTED)\n", (int)st);
      }
      else
      {
        Serial.println("[OTA] esp_ota_get_state_partition() failed");
      }
    }

    Serial.printf("[OTA] Next partition: %s| Size partition: %u\n", next_p ? next_p->label : "unknown", next_p->size);
  }
}
void OTA::bootGuard()
{
  early_bootloop_guard();
  log_reset_reason();
  log_ota_state();
  Get_partition_Valid();
}

String OTA::Get_running_partition()
{
  const esp_partition_t *p = esp_ota_get_running_partition();
  return p ? String(p->label) : String("unknown");
}
String OTA::Get_nextBoot_partition()
{
  const esp_partition_t *p = esp_ota_get_boot_partition();
  return p ? String(p->label) : String("unknown");
}
String OTA::Get_nextUpdate_partition()
{
  const esp_partition_t *p = esp_ota_get_next_update_partition(NULL);
  return p ? String(p->label) : String("unknown");
}

void callBackTimerVerify(TimerHandle_t xTimer)
{
  // Só faz rollback se ainda estiver pendente de verificação
  const esp_partition_t* p = esp_ota_get_running_partition();
  esp_ota_img_states_t st;
  if (p && esp_ota_get_state_partition(p, &st) == ESP_OK &&
      st == ESP_OTA_IMG_PENDING_VERIFY)
  {
    Serial.println("[OTA] Imagem OTA nao validada no prazo → rollback agora");
    esp_ota_mark_app_invalid_rollback_and_reboot(); // nao retorna
  }

  Serial.println("callback timer acionado");
  // Se chegou aqui, ou já validou, ou não há estado: só encerra o timer
  xTimerDelete(xTimer, 0);
}
void bootVerify(void * pvParameters)
{
    // Cria e inicia timer one-shot (30s)
  if (!xTimerVerify) {
    xTimerVerify = xTimerCreate("TimerVerify", pdMS_TO_TICKS(30000), pdFALSE, 0, callBackTimerVerify);
    if (xTimerVerify) xTimerStart(xTimerVerify, 0);
  }

  while (1)
  {
      const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t st = (esp_ota_img_states_t)0;
  esp_err_t err = esp_ota_get_state_partition(running, &st);
  if (err == ESP_OK)
  {
        if (st==ESP_OTA_IMG_VALID)
    {
       esp_err_t ok = esp_ota_mark_app_valid_cancel_rollback();
      Serial.printf("[OTA] mark_app_valid_cancel_rollback(): %s\n", esp_err_to_name(ok));
      ota_object.zerar_contador_boot();
      Serial.println("[BOOT] Inicializacao concluida. Sistema pronto.");
      xTimerStop(xTimerVerify, 0);
      xTimerDelete(xTimerVerify,0);
      vTaskDelete(xBootVerify);

    }
  }
  if (st!=ESP_OTA_IMG_VALID)
  {
    if ((int)st==(-1))
    {
       Serial.printf("[OTA] get_state_partition falhou: %s (provavel boot via serial)\n", esp_err_to_name(err));
      xTimerStop(xTimerVerify, 0);
      xTimerDelete(xTimerVerify,0);
      vTaskDelete(xBootVerify);
    }
    else
    {
      Serial.printf("[OTA] Image state: %d (0=INVALID,1=VALID,2=PENDING_VERIFY,3=ABORTED)\n", (int)st);

    }
    
    
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
  }

  
}
void OTA::Get_partition_Valid()
{
  
    xTimerVerify = xTimerCreate("TimerVerify", pdMS_TO_TICKS(30000), pdFALSE, 0, callBackTimerVerify);
    if(xTimerVerify)
    {
      Serial.println("Timer RTOS 30S criado");
    }
    else
    {
      Serial.println("Timer RTOS 30S não foi criado");
    }
    
     xTaskCreatePinnedToCore(
      bootVerify,          // Função que representa a tarefa
      "bootVerify",        // Nome da tarefa
      4096,             // Tamanho da pilha da tarefa
      NULL,             // Parâmetro da tarefa
      configMAX_PRIORITIES - 1,                // Prioridade da tarefa (5 é mais alta, portanto é uma função prioritaria)
      &xBootVerify,   // Variável para armazenar o identificador da tarefa
      0);               // Núcleo da CPU para atribuir a tarefa

}

OTA::OTA(const char *ssid,
         const char *senha,
         unsigned long timerS)
    : AP_SSID(ssid), AP_PASS(senha), timer(timerS)
{
}

void callBackTimerWifi(TimerHandle_t xTimer)
{
  // Desliga o AP e libera o rádio (opcional desligar o WiFi)
  WiFi.softAPdisconnect(true);
  // WiFi.mode(WIFI_OFF); // opcional, se quiser derrubar tudo
  Serial.println("[TimerWifi] AP desligado.");
}
void OTA::WifiAP(unsigned long timer)
{
  unsigned long tempoMS = timer * 1000;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS); // 2º arg = senha
  Serial.println();
  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP IP  : ");
  Serial.println(WiFi.softAPIP());

  if (tempoMS)
  {
    xTimerWifi = xTimerCreate("TimerWifi", pdMS_TO_TICKS(tempoMS), pdFALSE, 0, callBackTimerWifi);
    xTimerStart(xTimerWifi, 0);
    Serial.printf("[TimerWifi] AP iniciado.(%d s)\n", timer);
  }
}

void OTA::handleUpload(AsyncWebServerRequest *request, String filename,
                       size_t index, uint8_t *data, size_t len, bool final)
{
  if (!web.autentication(request))
    return;

  if (index == 0)
  {
    Serial.printf("[OTA] Iniciando upload: %s\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
      Update.printError(Serial);
    }
  }
  if (len)
  {
    if (Update.write(data, len) != len)
    {
      Update.printError(Serial);
    }
  }
  if (final)
  {
    if (Update.end(true))
    {
      Serial.printf("[OTA] Sucesso: %u bytes\n", (unsigned)index + len);
    }
    else
    {
      Update.printError(Serial);
    }
  }
}

void OTA::finishUpdate(AsyncWebServerRequest *request)
{
  if (!web.autentication(request))
    return;
  if (!Update.hasError())
  {
    request->send(200, "text/plain", "Atualização concluída! Reiniciando...");
    Serial.println("[OTA] Reiniciando...");
    delay(1500);
    ESP.restart();
    esp_restart();
  }
  else
  {
    request->send(500, "text/plain", "Falhou! Veja o log serial.");
  }
}
