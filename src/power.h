void powerOn() {}

void shutdown() {
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BUTTON_1, 0);  // 1 = High, 0 = Low
  esp_deep_sleep_start();
}

void reboot(char *args, Stream *response) { ESP.restart(); }
