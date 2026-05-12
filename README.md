# iot-hvac-controller

## Overview
An ESP32-based smart HVAC controller for industrial environments. Monitors temperature via DHT22 sensor, adjusts setpoint via potentiometer, and controls ventilation through a relay. Features an I2C LCD for real-time status display, emergency stop, overheat alarm, and sensor anomaly detection. Serial Plotter output enables live temperature and setpoint graphing.

## Features
- FreeRTOS multi-task architecture (Sensor, Control, Log)
- DHT22 temperature monitoring with exponential moving average filter
- 12-bit ADC potentiometer for adjustable setpoint (0–50°C)
- I2C LCD display for real-time temperature and system status output
- Interrupt-driven emergency stop button with software debounce (200ms)
- Dual-threshold safety: adjustable setpoint + fixed 35°C overheat alarm
- Fail-safe relay (de-energises on fault/emergency)
- Buzzer and LED status indicators
- Sensor anomaly detection (NaN, out-of-range, sudden jump)
- Watchdog timer (5-second timeout)
- Mutex-protected shared SystemState struct
- Serial diagnostic logging with Serial Plotter graph support

## Hardware
| Component | GPIO Pin |
|-----------|----------|
| DHT22 Temperature Sensor | 4 |
| Potentiometer | 34 |
| Emergency Stop Button | 15 |
| Relay Module | 23 |
| Buzzer | 19 |
| LED (with 1kΩ resistor) | 16 |
| LCD Display (I2C) | 21 (SDA), 22 (SCL) |

## Software Structure
- **Sensor Task** (Priority 2, 2s cycle): Reads DHT22 and ADC, applies filter, detects anomalies
- **Control Task** (Priority 3, 200ms cycle): Manages relay, buzzer, LED based on temperature and safety state
- **Log Task** (Priority 1, 1s cycle): Updates LCD display, outputs system status to serial monitor and Serial Plotter
- **SystemState struct**: Holds temperature, setpoint, ADC value, emergency stop flag
- **Mutex**: Protects all shared data access between tasks

## Simulation
Built and tested in Wokwi. Open `sketch.ino` and `diagram.json` to run.

## Industrial Safety
- Latching emergency stop (cannot be cleared by software)
- Fail-safe relay defaults to OFF on power loss
- Independent 35°C overheat alarm regardless of setpoint
- Watchdog triggers full reset if any task hangs
- Sensor anomaly auto-triggers emergency stop

## Wokwi Link
https://wokwi.com/projects/462472464187038721

## Author
Aaron Jones – 24027633
