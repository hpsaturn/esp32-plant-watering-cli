/**
 * @file main.cpp
 * @author Antonio Vanegas @hpsaturn
 * @date April 2025
 * @brief Basil Plant CLI - Water Pump Controller
 * @details This program controls a water pump for a plant using an ESP32 microcontroller.
 * @link https://github.com/hpsaturn/esp32-plant-watering-cli @endlink
 * @license GPL3
 */

#include <ESP32Servo.h>
#include <ESP32WifiCLI.hpp>
#include <EasyPreferences.hpp> 
#include <OTAHandler.h>

#include "OneButton.h"
#include "alarm_manager.h"
#include "esp_sntp.h"
#include "logo.h"
#include "app_config.h"
#include "power.h"

// #define PUMP_TYPE_SERVO 1

// Global alarm manager instance
AlarmManager alarmManager;

OneButton button1(PIN_BUTTON_1, true);

const char *key_ntp_server = "kntpserver";
const char *key_tzone = "ktzone";

bool wcli_setup_ready = false;

Servo pumpServo1, pumpServo2;

/**
 * @brief Callback class for ESP32WifiCLI
 * @details This class handles the WiFi status and command line interface (CLI) events.
 */
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

  // TODO: migrate it to class object
  #ifdef PUMP_TYPE_SERVO
  pumpServo1.attach(PIN_PUMP_1);
  pumpServo2.attach(PIN_PUMP_2);
  pumpServo1.write(angle.toInt());
  pumpServo2.write(angle.toInt());
  delay(time.toDouble());  // TODO: migrate it to task (not blocking)
  pumpServo1.write(PUMP_ANGLE_STOP);
  pumpServo2.write(PUMP_ANGLE_STOP);
  pumpServo1.detach();
  pumpServo2.detach();
  #else
  digitalWrite(PIN_PUMP_1, HIGH);
  digitalWrite(PIN_PUMP_2, HIGH);
  delay(time.toDouble());  // TODO: migrate it to task (not blocking)
  digitalWrite(PIN_PUMP_1, LOW);
  digitalWrite(PIN_PUMP_2, LOW); 
  #endif
}

/**
 * @brief Callback function for alarm triggered event
 * @param alarmName Name of the triggered alarm
 * @param timeinfo Pointer to the tm structure containing the current time
 */
void alarmTriggered(const char *alarmName, const tm *timeinfo) {
  Serial.printf("\r\nALARM TRIGGERED [%02d:%02d]: %s\r\n", timeinfo->tm_hour, timeinfo->tm_min,
                alarmName);
  enablePump((char *)"250 30000", &Serial);  // enable pump for 15 second
}

/**
 * @brief test the pump
 */
void testPump() { enablePump((char *)"120 15000", &Serial); }

/**
 * @brief update the time settings
 * @details This function configures the NTP server and timezone settings.
 */
void updateTimeSettings() {
  String server = wcli.getString(key_ntp_server, NTP_SERVER1);
  String tzone = wcli.getString(key_tzone, DEFAULT_TZONE);
  Serial.printf("ntp server: \t%s\r\ntimezone: \t%s\r\n", server.c_str(), tzone.c_str());
  configTime(GMT_OFFSET_SEC, DAY_LIGHT_OFFSET_SEC, server.c_str(), NTP_SERVER2);
  setenv("TZ", tzone.c_str(), 1);
  tzset();
}

/**
 * @brief set the NTP server
 * @param args NTP server string
 * @param response Stream to send response to Serial or Telnet console
 * @details The command format is: ntpserver <server>
 */
void setNTPServer(char *args, Stream *response) {
  Pair<String, String> operands = wcli.parseCommand(args);
  String server = operands.first();
  if (server.isEmpty()) {
    Serial.println(wcli.getString(key_ntp_server, NTP_SERVER1));
    return;
  }
  wcli.setString(key_ntp_server, server);
  updateTimeSettings();
}

/**
 * @brief set the timezone for the NTP server
 * @param args timezone string. Please refer to https://tinyurl.com/4s44uyzn
 * @param response Stream to send response to Serial or Telnet console
 * @details The command format is: ntpzone <timezone>
 */
void setTimeZone(char *args, Stream *response) {
  Pair<String, String> operands = wcli.parseCommand(args);
  String tzone = operands.first();
  if (tzone.isEmpty()) {
    Serial.println(wcli.getString(key_tzone, DEFAULT_TZONE));
    return;
  }
  wcli.setString(key_tzone, tzone);
  updateTimeSettings();
}

/**
 * @brief show the current local time and configured alarms
 * @param args Command line arguments (not used)
 * @param response Stream to send response to Serial or Telnet console
 */
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

  for (const auto &alarm : alarmManager.getAlarms()) {
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

    response->printf("%02d:%02d - %-20s %s\r\n", alarm.hour, alarm.minute, alarm.name,
                     status.c_str());
  }

  if (alarmManager.getAlarms().empty()) {
    response->println("No alarms configured");
    response->println("Use 'addalarm HH:MM Name' to add one");
  }
  response->println("----------------------------");
}

/**
 * @brief Add an alarm to the alarm manager
 * @param args Command line arguments (HH:MM Alarm Name)
 * @param response Stream to send response to Serial or Telnet console
 * @details The command format is: addalarm HH:MM Alarm Name
 */
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
  int minute = timeStr.substring(colonPos + 1).toInt();

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
  alarmManager.saveAlarms();  // Save to preferences
  response->printf("Added alarm: %02d:%02d - %s\r\n", hour, minute, safeName);
}

/**
 * @brief Drop an alarm from the alarm manager
 * @param args Command line arguments (Alarm Name)
 * @param response Stream to send response to Serial or Telnet console
 * @details The command format is: dropalarm Alarm Name
 */
void dropAlarm(char *args, Stream *response) {
  Pair<String, String> operands = wcli.parseCommand(args);
  String name = operands.first();
  String second = operands.second();

  if (!second.isEmpty()) {
    name = name + " " + second;
  }

  if (name.isEmpty()) {
    response->println("Usage: dropalarm Alarm Name");
    return;
  }

  if (alarmManager.deleteAlarmByName(name.c_str())) {
    alarmManager.saveAlarms();  // Save to preferences
    response->printf("Removed alarm: %s\r\n", name.c_str());
  } else {
    response->printf("No alarm found with name: %s\r\n", name.c_str());
  }
}

/**
 * @brief check for triggered alarms
 * @details This function checks for triggered alarms every second.
 */
void checkAlarms() {
  static uint32_t last_tick;
  if (millis() - last_tick > 1000) {
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    alarmManager.checkAlarms(&timeinfo);
    last_tick = millis();
  }
}

/**
 * @brief get ADC value from a specified pin
 * @param args Command line arguments (PIN)
 * @param response Stream to send response to Serial or Telnet console
 * @details The command format is: getADCVal PIN
 */
void getADCVal(char *args, Stream *response) {
  Pair<String, String> operands = wcli.parseCommand(args);
  int pin = operands.first().toInt();
  if (pin < 0 || pin > 39) {
    response->println("Error: Invalid pin number (0-39)");
    return;
  }
  int adcVal = analogRead(pin);
  double voltage = adcVal / 4095.0 * 3.3;  //Convert to the voltage value at the detection point.
  double battery = voltage * 4.0;          //There is only 1/4 battery voltage at the detection point.
  response->printf("ADC Val: %d, \t Voltage: %.2fV, \t Battery: %.2fV\r\n", adcVal, voltage, battery);
}

void enableOTA() {
  ota.setup(WiFi.getHostname(), "basil_plant");
  ota.setOnUpdateMessageCb([](const char *msg) { Serial.println(msg); });
}

void initRemoteShell() {
#ifndef DISABLE_CLI_TELNET
  if (wcli.isTelnetRunning()) wcli.shellTelnet->attachLogo(logo);
#endif
}

void setup() {
  Serial.begin(115200);
  button1.attachClick([]() { testPump(); });

  cfg.init("basil_plant");
  
  wcli.setCallback(new mESP32WifiCLICallbacks());
  wcli.shell->attachLogo(logo);
  wcli.setSilentMode(true);
  // NTP init
  updateTimeSettings();
  // Initialize alarm callback
  alarmManager.setCallback(alarmTriggered);
  alarmManager.loadAlarms();
  // CLI config
  wcli.add("ntpserver", &setNTPServer, "\tset NTP server. Default: pool.ntp.org");
  wcli.add("ntpzone", &setTimeZone, "\tset TZONE. https://tinyurl.com/4s44uyzn");
  wcli.add("time", &printLocalTime, "\t\tprint the current time and alarms");
  wcli.add("reboot", &reboot, "\tbasil plant reboot");
  wcli.add("pumptest", &enablePump, "\t<PWM> <time (ms)> enable pump servo");
  wcli.add("addalarm", &addAlarm, "\t<HH:MM> <Alarm Name> add alarm");
  wcli.add("dropalarm", &dropAlarm, "\t<Alarm Name> remove alarm");
  wcli.add("getADCVal", &getADCVal, "\t<PIN> get ADC voltage");
  wcli_setup_ready = wcli.isConfigured();
  wcli.begin("basil_plant");
  initRemoteShell();

  if (wcli_setup_ready) enableOTA();

  // Allow allocation of all timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // Set up the pump servo
  pinMode(PIN_PUMP_1, GPIO_MODE_OUTPUT);
  pinMode(PIN_PUMP_2, GPIO_MODE_OUTPUT);
  digitalWrite(PIN_PUMP_1, LOW);
  digitalWrite(PIN_PUMP_2, LOW);
}

void loop() {
  wcli.loop();
  button1.tick();
  if (!wcli_setup_ready) return; // Only run services if WiFi setup is ready
  ota.loop();
  checkAlarms(); 
}
