#include "Arduino.h"
#include "OneButton.h"
#include "WiFi.h"
#include "pin_config.h"
#include "power.h"
#include "esp_sntp.h"
#include "time.h"
#include <ESP32WifiCLI.hpp>

const char logo[] =
"\033[0;32m╭──────────────────────╮\033[0m\r\n"
"\033[0;32m│       🌱 BASIL       │\033[0m\r\n"
"\033[0;32m│      _               │\033[0m\r\n"
"\033[0;32m│     / \\              │\033[0m\r\n"
"\033[0;32m│    / o \\   Fresh &   │\033[0m\r\n"
"\033[0;32m│   /_____\\   Green    │\033[0m\r\n"
"\033[0;32m│     |@|              │\033[0m\r\n"
"\033[0;32m│    / | \\             │\033[0m\r\n"
"\033[0;32m╰──────────────────────╯\033[0m\r\n"
"\033[0;32m╭──────────────────────╮\033[0m\r\n"
"\033[0;32m│  \033[1;33mBasil Plant CLI \u2122\033[0m   \033[0;32m│\r\n"
"\033[0;32m│  \033[1;33mVersion: 0.0.1\033[0m      \033[0;32m│\r\n"
"\033[0;32m│  \033[1;33m@hpsaturn\033[0m           \033[0;32m│\r\n"
"\033[0;32m│  \033[1;33m2025\033[0m                \033[0;32m│\r\n"
"\033[0;32m╰──────────────────────╯\033[0m\r\n"
;

OneButton button1(PIN_BUTTON_1, true);
OneButton button2(PIN_BUTTON_2, true);

const char * key_ntp_server = "kntpserver";
const char * key_tzone = "ktzone";

// change these params via CLI:
const char * default_server = "pool.ntp.org";  
const char * default_tzone = "CET-1CEST,M3.5.0,M10.5.0/3";

bool wcli_setup_ready = false;

class mESP32WifiCLICallbacks : public ESP32WifiCLICallbacks {
  void onWifiStatus(bool isConnected) {}
  void onHelpShow() {}
  void onNewWifi(String ssid, String passw) { wcli_setup_ready = wcli.isConfigured(); }
};

void updateTimeSettings() {
  String server = wcli.getString(key_ntp_server, default_server );
  String tzone = wcli.getString(key_tzone, default_tzone);
  Serial.printf("ntp server: \t%s\r\ntimezone: \t%s\r\n",server.c_str(),tzone.c_str());
  configTime(GMT_OFFSET_SEC, DAY_LIGHT_OFFSET_SEC, server.c_str(), NTP_SERVER2);
  setenv("TZ", tzone.c_str(), 1);  
  tzset();
}

void setNTPServer(char *args, Stream *response) {
  Pair<String, String> operands = wcli.parseCommand(args);
  String server = operands.first();
  if (server.isEmpty()) {
    Serial.println(wcli.getString(key_ntp_server, default_server));
    return;
  }
  wcli.setString(key_ntp_server, server);
  updateTimeSettings();
}

void setTimeZone(char *args, Stream *response) {
  Pair<String, String> operands = wcli.parseCommand(args);
  String tzone = operands.first();
  if (tzone.isEmpty()) {
    Serial.println(wcli.getString(key_tzone, default_tzone));
    return;
  }
  wcli.setString(key_tzone, tzone);
  updateTimeSettings();
}

void printLocalTime(char *args, Stream *response) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No time available (yet)");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void initRemoteShell(){
  #ifndef DISABLE_CLI_TELNET 
    if (wcli.isTelnetRunning()) wcli.shellTelnet->attachLogo(logo);
  #endif
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  button1.attachClick([]() { shutdown(); });

  wcli.setCallback(new mESP32WifiCLICallbacks());
  wcli.shell->attachLogo(logo);
  wcli.setSilentMode(true);
  // NTP init
  updateTimeSettings();
  // CLI config  
  wcli.add("ntpserver", &setNTPServer, "\tset NTP server. Default: pool.ntp.org");
  wcli.add("ntpzone", &setTimeZone, "\tset TZONE. https://tinyurl.com/4s44uyzn");
  wcli.add("time", &printLocalTime, "\t\tprint the current time");
  wcli.add("reboot", &reboot, "\tbasil plant reboot");
  wcli_setup_ready = wcli.isConfigured();
  wcli.begin("basil_plant");
  initRemoteShell();
}

void loop() {
  button1.tick();
  // button2.tick();
  delay(3);
  static uint32_t last_tick;
  if (millis() - last_tick > 1000) {
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    // lv_msg_send(MSG_NEW_HOUR, &timeinfo.tm_hour);
    // lv_msg_send(MSG_NEW_MIN, &timeinfo.tm_min);
    // lv_msg_send(MSG_NEW_VOLT, &volt);
    last_tick = millis();
  }
  while(!wcli_setup_ready) wcli.loop(); // only for fist setup
  wcli.loop();
}
