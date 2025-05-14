#include <Preferences.h>

#include <cstring>
#include <vector>

#include "time.h"

typedef void (*AlarmCallback)(const char* alarmName, const tm* timeinfo);

class AlarmManager {
 private:
  struct Alarm {
    int hour;
    int minute;
    const char* name;
    bool triggered;

    // Constructor that copies the string
    Alarm(int h, int m, const char* n)
        : hour(h), minute(m), name(n ? strdup(n) : nullptr), triggered(false) {}

    // Destructor to free memory
    ~Alarm() {
      if (name) {
        free((void*)name);  // Cast away const
      }
    }

    // Rule of Three: Copy constructor
    Alarm(const Alarm& other)
        : hour(other.hour),
          minute(other.minute),
          name(strdup(other.name)),
          triggered(other.triggered) {}

    // Rule of Three: Copy assignment
    Alarm& operator=(const Alarm& other) {
      if (this != &other) {
        if (name) free((void*)name);
        hour = other.hour;
        minute = other.minute;
        name = strdup(other.name);
        triggered = other.triggered;
      }
      return *this;
    }
  };
  std::vector<Alarm> alarms;
  AlarmCallback callback = nullptr;

 public:
  void addDailyAlarm(int hour, int minute, const char* name) {
    alarms.push_back(Alarm{hour, minute, name});
  }

  void setCallback(AlarmCallback cb) { callback = cb; }

  // Get all alarms (const reference)
  const std::vector<Alarm>& getAlarms() const { return alarms; }

  void checkAlarms(const tm* timeinfo) {
    for (auto& alarm : alarms) {
      if (!alarm.triggered && timeinfo->tm_hour == alarm.hour && timeinfo->tm_min == alarm.minute) {
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

  void saveAlarms() {
    Preferences prefs;
    prefs.begin("alarm_manager", false);

    // Calculate total size needed
    size_t totalSize = 2;  // For alarm count (uint16_t)
    for (const auto& alarm : alarms) {
      totalSize += 2;                                        // hour + minute
      totalSize += 2;                                        // name length (uint16_t)
      totalSize += alarm.name ? strlen(alarm.name) + 1 : 0;  // name with null terminator
    }

    // Allocate buffer
    uint8_t* buffer = static_cast<uint8_t*>(malloc(totalSize));
    if (!buffer) {
      prefs.end();
      return;
    }

    size_t pos = 0;
    // Write alarm count
    uint16_t count = alarms.size();
    memcpy(buffer + pos, &count, sizeof(count));
    pos += sizeof(count);

    // Write each alarm
    for (const auto& alarm : alarms) {
      // Write hour and minute
      buffer[pos++] = static_cast<uint8_t>(alarm.hour);
      buffer[pos++] = static_cast<uint8_t>(alarm.minute);

      // Handle name
      const char* name = alarm.name;
      uint16_t nameLen = 0;
      if (name) {
        nameLen = static_cast<uint16_t>(strlen(name) + 1);  // Include null terminator
      }

      // Write name length and content
      memcpy(buffer + pos, &nameLen, sizeof(nameLen));
      pos += sizeof(nameLen);
      if (name && nameLen > 0) {
        memcpy(buffer + pos, name, nameLen);
        pos += nameLen;
      }
    }

    // Save to preferences
    prefs.putBytes("alarms", buffer, totalSize);
    free(buffer);
    prefs.end();
  }

  void loadAlarms() {
    Preferences prefs;
    prefs.begin("alarm_manager", true);  // Read-only

    size_t size = prefs.getBytesLength("alarms");
    if (size == 0) {
      prefs.end();
      return;
    }

    uint8_t* buffer = static_cast<uint8_t*>(malloc(size));
    if (!buffer) {
      prefs.end();
      return;
    }

    prefs.getBytes("alarms", buffer, size);
    prefs.end();

    size_t pos = 0;
    alarms.clear();

    // Read alarm count
    uint16_t count;
    memcpy(&count, buffer + pos, sizeof(count));
    pos += sizeof(count);

    // Read each alarm
    for (uint16_t i = 0; i < count; i++) {
      if (pos + 4 > size) break;  // Check for header corruption

      // Read hour and minute
      uint8_t hour = buffer[pos++];
      uint8_t minute = buffer[pos++];

      // Read name length
      uint16_t nameLen;
      memcpy(&nameLen, buffer + pos, sizeof(nameLen));
      pos += sizeof(nameLen);

      // Read name
      const char* name = nullptr;
      if (nameLen > 0 && (pos + nameLen) <= size) {
        name = reinterpret_cast<const char*>(buffer + pos);
        pos += nameLen;
      }

      // Add alarm (constructor will copy the name)
      alarms.emplace_back(hour, minute, name);
    }

    free(buffer);
  }
};