# NASA cFS & STM32 Telemetry Bridge Implementation

# About
This repo documents a embedded flight-software integration project demonstrating communication between NASA's Core Flight System (cFS) and an STM32 microcontroller and DHT11 temperature/humidity sensor using CCSDS Space Packet Protocol telemetry packets. 

# What is NASA Core Flight System?
NASA's Core Flight System (cFS) is a reusable, platform-independent flight software framework designed for embedded real-time systems. Its architecture separates applications from the underlying operating system and hardware, making flight software more portable and reusable.

The primary components include:

| Component | Description |
|---|---|
| **cFE — core Flight Executive** | Provides the application runtime environment and common flight-software services. |
| **OSAL — Operating System Abstraction Layer** | Provides a consistent API between cFS applications and the underlying operating system. |
| **PSP — Platform Support Package** | Provides an abstraction between cFS and the target hardware platform. |

For this project, cFS provides the flight software side of the system, while the STM32 and DHT11 temperature/humidity sensor acts as the embedded subsystem.

# How The STM32 Is Integrated?
CCSDS Space Packets provide a standardized structure for exchanging spacecraft telemetry and command data. In this project, sensor measurements from the STM32 are packaged into telemetry data and transferred to the cFS flight software environment.

<br/>
<img width="425" height="247" alt="image" src="https://github.com/user-attachments/assets/eeb8c898-881b-4c8b-900e-439ec779c6ce" />
<br/>

Each packet consists of a primary header, secondary header, and telemetry frame that gets decoded and transported to specific applications based on message IDs. The specific structure is shown below per header based on the [CCSDS Space Packet Protocol Blue Manual](https://ccsds.org/wp-content/uploads/gravity_forms/5-448e85c647331d9cbaf66c096458bdd5/2025/01//133x0b2e2.pdf?gv-iframe=true):

<img width="723" height="300" alt="image" src="https://github.com/user-attachments/assets/33561f52-4940-4b19-a5bd-9446dae2d1aa" />

<img width="426" height="250" alt="image" src="https://github.com/user-attachments/assets/e0198e19-e0c6-4ef8-a025-cb5ae261545a" />

# How To Build and Run
To make building and running this implementation as easy as possible, I provided two scripts that can be called from the cFS directory that will either build or run cFS. 

First, download PlatformIO to upload the STM32 software inside STM32_Bridge to the board, or use Arduino IDE. 

``` bash
Navigate to cFS location from this repo,

To build:

./build.sh

To run:

./run.sh
```

If everything is running correctly, the terminal should show cFS posting commands and a GUI should appear. Click 'Enable Telemetry' at destination IP 127.0.0.1 to let data from the TO_LAB app to be transmitted out of cFS to the GUI via UDP.

# Results
Below shows the working setup, where a STM32 connected to cFS can sucessfully send CCSDS packets to CI_LAB, and out to TO_LAB to mimic how a real aerospace system works.

<img width="1851" height="1055" alt="image" src="https://github.com/user-attachments/assets/37660e54-ff2b-4e99-8cde-6ada724127d9" />

# Conclusion
Feel free to clone the repo and add your own features to this demo!
