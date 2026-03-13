# BasicTeamsLight

A simple Teams/microphone status light using ESP32 — no Microsoft Graph API required.  
![Image Alt](https://github.com/Doppelp0int/BasicTeamsLight/blob/main/img/Image.jpg?raw=true)
---

## Overview

This project uses an ESP32 and a WS2812 LED ring to display your status (Available, In Call, Away, Offline) via a web interface. It also allows WiFi configuration, brightness adjustment (in %), and visual indication of the current state.

---

## Components

ESP32 Dev Board — Main controller — [Amazon Link](https://amzn.to/3OZXNXJ)  
WS2812 LED Ring (12 LEDs) — RGB LEDs for status display — [Amazon Link](https://amzn.to/4cNdxHq)  
Cables — [Amazon Link](https://amzn.to/3OZXQTp)  

---

## Software / Libraries

Arduino IDE 2.3.2  
FastLED 3.10.3  
WebServer (built-in ESP32)  
Preferences (built-in ESP32)  
DNSServer (built-in ESP32)  

---

## Features

Four status modes with corresponding LED colors: Available – Green, In Call – Red, Away – Yellow (255, 255, 0), Offline – Off
Web interface to change status, configure WiFi, and adjust brightness (displayed in %)  
WiFi network scan with signal strength in %  
Blue scanning light when disconnected/searching for AP, pulsing fallback AP light during new AP setup  
Stores WiFi credentials and brightness in flash memory

![Image Alt](https://github.com/Doppelp0int/BasicTeamsLight/blob/main/img/1.png?raw=true)
---

## Wiring

WS2812 Data → ESP32 Pin 5  
WS2812 VCC → 5V Power Supply  --> ESP32 VCC
WS2812 GND → ESP32 GND

**Note:** Each LED can draw ~60mA at full brightness. Make sure your PSU can handle the total current.  

---

## Usage

On first boot without saved WiFi credentials, ESP32 creates an AP: `BasicTeamsLamp` Password: 12345678  
Connect to the AP and open browser at `192.168.4.1` (Please connect from your Phone, on PC it sometimes does not work!)
Select WiFi network, enter password, set brightness, and save. ESP32 will restart  
After connecting to WiFi, open the web interface at the ESP32 IP printed in Serial Monitor  
Click buttons to change LED status; the colored circle shows the currently active status  
Adjust brightness with the slider (shown in %)  
The PowerShell script will send the HTTP POST Requests, see bellow...

---

## PowerShell Command Script (only Works on Windows 11)

You can send status commands to the ESP32 using PowerShell. Save this as `sendStatus.ps1` in your **Autostart** folder. It will monitor your microphone state and automatically send the current status to the ESP32.

- **av** – when your microphone is not active  
- **in_call** – when the microphone icon is active (you are in a call)  
- **away** – if you leave your PC idle for 2 minutes or lock your PC  
- **offline** – when you shut down your PC (note: this may not always work)
