#include "OTAHandler.h"

OTAHandler::OTAHandler() { m_pOTAHandlerCallbacks = nullptr; }

void OTAHandler::setup(const char* ESP_ID, const char* ESP_PASS) {
  _ESP_ID = ESP_ID;
  _ESP_PASS = ESP_PASS;
  _baud = 1500000;
  ArduinoOTA.setHostname(_ESP_ID);
  ArduinoOTA.setPassword(_ESP_PASS);

  ArduinoOTA
      .onStart([]() {
        Serial.printf("\r\n");
        if (ota.getInstance()->m_pOTAHandlerCallbacks != nullptr)
          ota.getInstance()->m_pOTAHandlerCallbacks->onStart();
      })
      .onEnd([]() {
        Serial.println("\r\n[OTA] success!");
        if (ota.getInstance()->m_pOTAHandlerCallbacks != nullptr)
          ota.getInstance()->m_pOTAHandlerCallbacks->onEnd();
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("\r[OTA] Progress: %u%%", (progress / (total / 100)));
        if (ota.getInstance()->m_pOTAHandlerCallbacks != nullptr)
          ota.getInstance()->m_pOTAHandlerCallbacks->onProgress(progress, total);
      })
      .onError([](ota_error_t error) {
        Serial.printf("[E][OTA] Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed");
        if (ota.getInstance()->m_pOTAHandlerCallbacks != nullptr)
          ota.getInstance()->m_pOTAHandlerCallbacks->onError();
      });

  ArduinoOTA.begin();
  log_i("local OTA updates on\t: %s passw: %s", ESP_ID, ESP_PASS);
}

void OTAHandler::loop() { ArduinoOTA.handle(); }

void OTAHandler::setBaud(int baud) { _baud = baud; }

void OTAHandler::setCallbacks(OTAHandlerCallbacks* pCallbacks) {
  m_pOTAHandlerCallbacks = pCallbacks;
}

void OTAHandler::setOnUpdateMessageCb(voidMessageCbFn cb) { _onUpdateMsgCb = cb; }

OTAHandler* OTAHandler::getInstance() { return this; }

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_OTAHANDLER)
OTAHandler ota;
#endif
