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
};