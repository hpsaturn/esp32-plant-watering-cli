/**
 * @file main.cpp
 * @author Antonio Vanegas @hpsaturn
 * @date April 2025
 * @brief Basil Plant CLI - Water Pump Controller
 * @details This program controls a water pump for a plant using an ESP32 microcontroller.
 * @link https://github.com/hpsaturn/esp32-plant-watering-cli @endlink
 * @license GPL3
 */

#include "Arduino.h"
#include "OneButton.h"
#include "WiFi.h"
#include "pin_config.h"
#include "power.h"
#include "esp_sntp.h"
#include "alarm_manager.h"
#include "logo.h"
#include <ESP32WifiCLI.hpp>
#include <ESP32Servo.h>

// Global alarm manager instance
AlarmManager alarmManager;

OneButton button1(PIN_BUTTON_1, true);

const char * key_ntp_server = "kntpserver";
const char * key_tzone = "ktzone";

// change these params via CLI:
const char * default_server = "pool.ntp.org";  
const char * default_tzone = "CET-1CEST,M3.5.0,M10.5.0/3";

bool wcli_setup_ready = false;

Servo pumpServo;
int servoPin = GPIO_NUM_21;

class mESP32WifiCLICallbacks : public ESP32WifiCLICallbacks {
  void onWifiStatus(bool isConnected) {}
  void onHelpShow() {}
  void onNewWifi(String ssid, String passw) { wcli_setup_ready = wcli.isConfigured(); }
};

void enablePump(char *args, Stream *response) {
  Pair<String, String> operands = wcli.parseCommand(args);
  String angle = operands.first();
  String time = operands.second();
  response->printf("Pump enabled for %s PWM for %s ms\r\n", angle.c_str(), time.c_str());
  pumpServo.attach(servoPin);
  pumpServo.write(angle.toInt());
  delay(time.toDouble());
  pumpServo.write(10);
  pumpServo.detach();
}

// Alarm callback function
void alarmTriggered(const char* alarmName, const tm* timeinfo) {
  Serial.printf("\r\nALARM TRIGGERED [%02d:%02d]: %s\r\n", timeinfo->tm_hour, timeinfo->tm_min, alarmName);
  enablePump((char *) "120 15000", &Serial);  // enable pump for 15 second
}

void testPump() {
  enablePump((char *) "120 15000", &Serial);
}

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
    response->println("No time available (yet)");
    return;
  }
  response->println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  response->println("----------------------------");

  // Print alarm list
  response->println("Configured Alarms:");
  response->println("----------------------------");
  
  bool foundNext = false;
  time_t now = mktime(&timeinfo);
  
  for (const auto& alarm : alarmManager.getAlarms()) {
    // Create tm struct for alarm time today
    struct tm alarmTime = timeinfo;
    alarmTime.tm_hour = alarm.hour;
    alarmTime.tm_min = alarm.minute;
    alarmTime.tm_sec = 0;
    time_t alarmToday = mktime(&alarmTime);
    
    // Calculate time difference
    double diff = difftime(alarmToday, now);
    int hoursRemaining = static_cast<int>(diff) / 3600;
    int minsRemaining = (static_cast<int>(diff) % 3600) / 60;

    // Determine status indicator
    String status;
    if (timeinfo.tm_hour == alarm.hour && timeinfo.tm_min == alarm.minute) {
      status = "🔔 ACTIVE NOW";
    } else if (diff > 0 && !foundNext) {
      status = "⏰ NEXT (" + String(hoursRemaining) + "h " + String(minsRemaining) + "m)";
      foundNext = true;
    } else if (diff > 0) {
      status = "🕒 Later (" + String(hoursRemaining) + "h " + String(minsRemaining) + "m)";
    } else {
      status = "✅ Passed today";
    }

    response->printf("%02d:%02d - %-20s %s\r\n", 
                 alarm.hour, alarm.minute, 
                 alarm.name, 
                 status.c_str());
  }
  
  if (alarmManager.getAlarms().empty()) {
    response->println("No alarms configured");
    response->println("Use 'addalarm HH:MM Name' to add one");
  }
  response->println("----------------------------");
}

// Add new CLI command for alarm management
void addAlarm(char *args, Stream *response) {
  Pair<String, String> operands = wcli.parseCommand(args);
  String timeStr = operands.first();
  String name = operands.second();
  
  int colonPos = timeStr.indexOf(':');
  if (colonPos == -1 || name.isEmpty()) {
      response->println("Usage: addalarm HH:MM Alarm Name");
      return;
  }
  
  int hour = timeStr.substring(0, colonPos).toInt();
  int minute = timeStr.substring(colonPos+1).toInt();

  // Validate time range
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    response->println("Error: Invalid time format (use HH:MM, 00-23:00-59)");
    return;
  }

  // Create safe copy of the name
  const size_t MAX_NAME_LENGTH = 32;
  char safeName[MAX_NAME_LENGTH] = {0};
  size_t copyLength = min(name.length(), MAX_NAME_LENGTH - 1);
  memcpy(safeName, name.c_str(), copyLength);
  safeName[copyLength] = '\0';  // Ensure null-termination

  alarmManager.addDailyAlarm(hour, minute, safeName);
  response->printf("Added alarm: %02d:%02d - %s\r\n", hour, minute, safeName);
}

void initRemoteShell(){
  #ifndef DISABLE_CLI_TELNET 
    if (wcli.isTelnetRunning()) wcli.shellTelnet->attachLogo(logo);
  #endif
}



void setup() {
  Serial.begin(115200);
  delay(2000);
  
  button1.attachClick([]() { testPump(); });

  wcli.setCallback(new mESP32WifiCLICallbacks());
  wcli.shell->attachLogo(logo);
  wcli.setSilentMode(true);
  // NTP init
  updateTimeSettings();
  // Initialize alarm callback
  alarmManager.setCallback(alarmTriggered);
  
  // Add example alarms (modify as needed)
  alarmManager.addDailyAlarm(8, 0, "Morning wakeup");
  alarmManager.addDailyAlarm(11, 0, "Morning watering");
  alarmManager.addDailyAlarm(20, 0, "Night shutdown");

  // CLI config  
  wcli.add("ntpserver", &setNTPServer, "\tset NTP server. Default: pool.ntp.org");
  wcli.add("ntpzone", &setTimeZone, "\tset TZONE. https://tinyurl.com/4s44uyzn");
  wcli.add("time", &printLocalTime, "\t\tprint the current time and alarms");
  wcli.add("reboot", &reboot, "\tbasil plant reboot");
  wcli.add("pumptest", &enablePump, "\t<PWM> <time (ms)> enable pump servo");
  wcli.add("addalarm", &addAlarm, "\t<HH:MM> <Alarm Name> add alarm");
  wcli_setup_ready = wcli.isConfigured();
  wcli.begin("basil_plant");
  initRemoteShell();

  // Allow allocation of all timers
	ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
  pumpServo.attach(servoPin);
}

void loop() {
  button1.tick();
  delay(5);
  static uint32_t last_tick;
  if (millis() - last_tick > 1000) {
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    alarmManager.checkAlarms(&timeinfo);
    last_tick = millis();
  }
  while(!wcli_setup_ready) wcli.loop(); // only for fist setup
  wcli.loop();
}
