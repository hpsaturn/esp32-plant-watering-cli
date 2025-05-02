#pragma once

#define WIFI_CONNECT_WAIT_MAX (30 * 1000)

// change these params via CLI:
#define DEFAULT_TZONE "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER1 "pool.ntp.org"

#define NTP_SERVER2 "time.nist.gov"
#define GMT_OFFSET_SEC (3600 * 8)
#define DAY_LIGHT_OFFSET_SEC 0

#define PIN_BUTTON_1 0

#define PIN_MOISTURE 14

#define PIN_IIC_SCL 17
#define PIN_IIC_SDA 18
