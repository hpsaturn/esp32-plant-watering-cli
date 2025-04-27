# ESP32 Plant Watering CLI

Scheduler and pump driver using a basic command line over an ESP32

![esp32 plant watering collage](images/collage.jpg)

## TODO

- [x] WiFi CLI Manager added
- [x] Hardcoded scheduler
- [x] Dynamic scheduler on memory
- [ ] Dynamic scheduler on flash
- [ ] Misture sensor via Bluetooth LE connection
- [ ] Auto plant watering mode

## Commands

For now, these are the available commands:

```shell
basil_plant:$ help

---- Shortcut Keys ----

Ctrl-A : Jumps the cursor to the beginning of the line.
Ctrl-E : Jumps the cursor to the end of the line.
Ctrl-D : Log Out.
Ctrl-R : Reverse-i-search.
Pg-Up  : History search backwards and auto completion.
Pg-Down: History search forward and auto completion.
Home   : Jumps the cursor to the beginning of the line.
End    : Jumps the cursor to the end of the line.

---- Available commands ----

addalarm: 	add alarm in HH:MM "Name" format
nmcli: 		network manager CLI. Type nmcli help for more info
ntpserver: 	set NTP server. Default: pool.ntp.org
ntpzone: 	set TZONE. https://tinyurl.com/4s44uyzn
pumptest: 	<PWM> <time (ms)> enable pump servo
reboot: 	basil plant reboot
time: 		print the current time
```

## Hardware

![esp32 plant watering](images/collage_hardware.jpg)

Using a simple servo controller and this pump from Aliexpress is enough for now. Possible compatible water pump:

- [Micro M20 Water Pump DC 3.7-5V](https://s.click.aliexpress.com/e/_okmECet)
- [Micro Mini 20mm 130 Motor Vacuum Air Pump DC 3V](https://s.click.aliexpress.com/e/_omedpXf)