# Prototype v1.0 solution IoT - Ouivalo

This code comes with this prototype board and must be implemented in an **Arduino MKR FOX 1200** : https://oshwlab.com/Thejulfi/final_iot_ouivalo

/!\ : The following documentation mainly concerns the C code (stored in `OUIVALO_IoT/Sigfox_comm/`).

## 1. What does it do?

This project is an **energy-efficient** RFID smart lock for composter bins. The lock is driven by a **motor**, and the "open" and "closed" states are recognized thanks to **two end switches**.

*Energy-efficient means that the whole system should run on batteries for at least two month.*

The Arduino MKR FOX 1200 and the **DHT22** send data to a Sigfox backend.

The operating routine work as follow : 
1. Someone wakes up the system,
2. An RFID badge is detected,
3. Latch opening and data transmission through Sigfox,
4. The system goes to sleep,
5. Compost's Bin closing,
6. Latch closing and the system goes back to sleep.

The goal of such a system is to modernize a composter bins fleet, and gathering data such as temperature, humidity, UID and hour of opening, and so on.
## 2. Installation of the project
### 2.1 Install using Visual Studio Code

1. Clone the repository in your platformio folder : `path_to_platformio/PlatformIO/Projects/`
2. Open Platformio and the project folder
3. Congrats, you have now access to the source of the project in `OUIVALO_IoT/Sigfox_comm/src/`

### 2.1 Install using Arduino IDE

1. Copy what's inside `OUIVALO_IoT/Sigfox_comm/src/` in `Your_arduinoIDE_project/`
2. Congrats, now you just need to open your project with Arduino IDE

## 4. How it works

### 4.1 Operating modes

The loop of this system works according to operating modes. There is 5 operating mode listed bellow : 

1. **INIT** : turning on all sensors,
2. **ON_MOTOR** : turing motors to close or open the latch (according to the ROUTINE_FLAG),
3. **ROUTINE** : attempt to detect an RFID card and message construction,
4. **SEND_MSG** : When the system send a message to the Sigfox backend,
5. **SLEEPING** : When the entire system goes into sleep mode.


## 5. Currently implemented and upcoming features

Features that are implemented for data feedback and future arrivals.

- [x] : Temperature and humidity sensor
- [x] : Battery level sensor
- [x] : UID 
- [ ] : Executing time sensor
- [ ] : fill level sensor

