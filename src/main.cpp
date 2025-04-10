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

#include <vector>

// Alarm callback function type
typedef void (*AlarmCallback)(const char* alarmName, const tm* timeinfo);

class AlarmManager {
private:
    struct Alarm {
        int hour;
        int minute;
        const char* name;
        bool triggered;
    };
    std::vector<Alarm> alarms;
    AlarmCallback callback = nullptr;
    
public:
    void addDailyAlarm(int hour, int minute, const char* name) {
        alarms.push_back({hour, minute, name, false});
    }

    void setCallback(AlarmCallback cb) {
        callback = cb;
    }

    // Get all alarms (const reference)
    const std::vector<Alarm>& getAlarms() const {
      return alarms;
    }

    void checkAlarms(const tm* timeinfo) {
        for (auto& alarm : alarms) {
            if (!alarm.triggered && 
                timeinfo->tm_hour == alarm.hour && 
                timeinfo->tm_min == alarm.minute) {
                
                alarm.triggered = true;
                if (callback) {
                    callback(alarm.name, timeinfo);
                }
            } 
            // Reset trigger at midnight
            else if (timeinfo->tm_hour == 0 && timeinfo->tm_min == 0) {
                alarm.triggered = false;
            }
        }
    }
};

// Global alarm manager instance
AlarmManager alarmManager;

// Example callback function
void alarmTriggered(const char* alarmName, const tm* timeinfo) {
    Serial.printf("ALARM TRIGGERED [%02d:%02d]: %s\n", 
                 timeinfo->tm_hour, timeinfo->tm_min, alarmName);
    // Add your alarm actions here
}

void updateTimeSettings() {
  String server = wcli.getString(key_ntp_server, default_server );
  String tzone = wcli.getString(key_tzone, default_tzone);
  Serial.printf("ntp server: \t%s\r\ntimezone: \t%s\r\n",server.c_str(),tzone.c_str());
  configTime(GMT_OFFSET_SEC, DAY_LIGHT_OFFSET_SEC, server.c_str(), NTP_SERVER2);
  setenv("TZ", tzone.c_str(), 1);  
  tzset();

  // Initialize alarm callback
  alarmManager.setCallback(alarmTriggered);
  // Add example alarms (modify as needed)
  alarmManager.addDailyAlarm(7, 0, "Morning wake-up");
  alarmManager.addDailyAlarm(12, 30, "Lunch time");
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

  Serial.println("----------------------------");

  // Print alarm list
  Serial.println("Configured Alarms:");
  Serial.println("----------------------------");
  
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

    Serial.printf("%02d:%02d - %-20s %s\n", 
                 alarm.hour, alarm.minute, 
                 alarm.name, 
                 status.c_str());
  }
  
  if (alarmManager.getAlarms().empty()) {
    Serial.println("No alarms configured");
    Serial.println("Use 'addalarm HH:MM \"Name\"' to add one");
  }
  Serial.println("----------------------------");
}

// Add new CLI command for alarm management
void addAlarm(char *args, Stream *response) {
  Pair<String, String> operands = wcli.parseCommand(args);
  String timeStr = operands.first();
  String name = operands.second();
  
  int colonPos = timeStr.indexOf(':');
  if (colonPos == -1 || name.isEmpty()) {
      Serial.println("Usage: addalarm HH:MM \"Alarm Name\"");
      return;
  }
  
  int hour = timeStr.substring(0, colonPos).toInt();
  int minute = timeStr.substring(colonPos+1).toInt();
  alarmManager.addDailyAlarm(hour, minute, name.c_str());
  Serial.printf("Added alarm: %02d:%02d - %s\n", hour, minute, name.c_str());
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
  wcli.add("addalarm", &addAlarm, "\tadd alarm in HH:MM \"Name\" format");
  wcli_setup_ready = wcli.isConfigured();
  wcli.begin("basil_plant");
  initRemoteShell();
  alarmManager.setCallback(alarmTriggered);
}

void loop() {
  button1.tick();
  // button2.tick();
  delay(3);
  static uint32_t last_tick;
  if (millis() - last_tick > 60000) {
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    alarmManager.checkAlarms(&timeinfo);
    last_tick = millis();
  }
  while(!wcli_setup_ready) wcli.loop(); // only for fist setup
  wcli.loop();
}
