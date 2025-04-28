#include "time.h"
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