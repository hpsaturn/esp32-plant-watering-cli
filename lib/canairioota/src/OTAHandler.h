#ifndef OTA_Handler_H
#define OTA_Handler_H

#include <ArduinoOTA.h>

typedef void (*voidMessageCbFn)(const char* msg);

class OTAHandlerCallbacks;
class OTAHandler {
 public:
  OTAHandler();
  void setup(const char* ESP_ID, const char* ESP_PASS);
  void setCallbacks(OTAHandlerCallbacks* pCallBacks);
  void setOnUpdateMessageCb(voidMessageCbFn cb);
  void loop();
  void setBaud(int baud);
  OTAHandler* getInstance();

 private:
  OTAHandlerCallbacks* m_pOTAHandlerCallbacks = nullptr;
  voidMessageCbFn _onUpdateMsgCb = nullptr;
  const char* _ESP_ID;
  const char* _ESP_PASS;
  int _baud;
  void remoteOTAcheckloop();
};

class OTAHandlerCallbacks {
 public:
  virtual ~OTAHandlerCallbacks(){};
  virtual void onStart();
  virtual void onProgress(unsigned int progress, unsigned int total);
  virtual void onEnd();
  virtual void onError();
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_OTAHANDLER)
extern OTAHandler ota;
#endif

#endif
