# Prototype v1.0 solution IoT - Ouivalo

This code comes with this prototype board and must be implemented in an **Arduino MKR FOX 1200** : https://oshwlab.com/Thejulfi/final_iot_ouivalo

## What does it do?

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

##  
