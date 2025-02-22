# Radio-Alarm
Two-part alarm clock communicating via radio waves

# Modules used
Transmitter:
  - nodeMCU v3 ESP8266 as MCU
  - nRF24L01 as radio module
  - 2x tactile switches
  - OLED display with I2C interface

Receiver:
  - nodeMCU v3 ESP8266 as MCU
  - nRF24L01 as radio module
  - DS1302 real-time clock
  - Passive buzzer 5V

# How it works?
Simple system designed for people having trouble getting up early morning.

On the transmitter side, user sets preferred alarm hour on the OLED display. Transmitter then sends a signal to a receiver, which checks the time in 45 seconds intervals. When the time sent by the signal and time fetched from RTC module match, a PWM signals are sent to the buzzer. To turn off the alarm user is required to go to the transmitter module and press both buttons.
