# NASA cFS & ESP32 Telemetry Bridge Implementation
<img width="4885" height="4005" alt="IMG_7980" src="https://github.com/user-attachments/assets/8d460c6f-1e6e-4392-9c7e-e8c40d517790" />

# About
This repo documents an embedded flight-software integration project demonstrating communication between NASA's Core Flight System (cFS) and an ESP32 microcontroller and DHT11 temperature/humidity sensor using CCSDS Space Packet Protocol.

# What is NASA Core Flight System?
NASA's Core Flight System (cFS) is a reusable, platform-independent flight software framework designed for embedded real-time systems. Its architecture separates applications from the underlying operating system and hardware, making flight software more portable and reusable.

The primary components include:

| Component | Description |
|---|---|
| **cFE — core Flight Executive** | Provides the application runtime environment and common flight-software services. |
| **OSAL — Operating System Abstraction Layer** | Provides a consistent API between cFS applications and the underlying operating system. |
| **PSP — Platform Support Package** | Provides an abstraction between cFS and the target hardware platform. |

## cFS Apps Running In This Project
For this project, cFS provides the flight software side of the system, while the ESP32 and DHT11 temperature/humidity sensor acts as the embedded subsystem.

| App | Description |
|---|---|
| **ESP32_BRIDGE_APP** | Reads CCSDS packets from ESP32 via serial. |
| **TO_LAB** | Outlet for telemetry data from cFS to external applications. |
| **CI_LAB** | Inlet for commands for cFS from external applications. |
| **LC** | Limit checker to observe packets for high temperature and humidity. |
| **SC** | Runs stored command sequences (RTS) when triggered. |
| **HS** | Health and safety monitors state of ESP32_BRIDGE_APP and safely recovers if needed. |
| **DS** | Data storage stores temperature and humidity telemetry into .dat files locally in cFS. |
| **FM** | File manager for onboard files, helps manage .dat files with temperature and humidity data. |

# What is CCSDS Space Packet Protocol?
CCSDS Space Packets provide a standardized structure for exchanging spacecraft telemetry and command data. In this project, sensor measurements from the ESP32 are packaged into telemetry data and transferred to the cFS flight software environment. Additionally, FreeRTOS is implemented to run tasks on separate processor cores concurrently such as the DHT11 sensor clock, SPI LCD display updates, and sending CCSDS packets via serial.

A CCSDS Space Packet has a primary header, with an optional secondary header and user data/application data. The specific structure is shown below per header based on the [CCSDS Space Packet Protocol Blue Manual](https://ccsds.org/wp-content/uploads/gravity_forms/5-448e85c647331d9cbaf66c096458bdd5/2025/01//133x0b2e2.pdf?gv-iframe=true):

<img width="723" height="300" alt="image" src="https://github.com/user-attachments/assets/33561f52-4940-4b19-a5bd-9446dae2d1aa" />

<img width="426" height="250" alt="image" src="https://github.com/user-attachments/assets/e0198e19-e0c6-4ef8-a025-cb5ae261545a" />

## Packet Structure
Below is the structure for a single CCSDS packet including any notable information.

| Bytes | Data | Notes | 
|---|---|---| 
| **0 - 5** | Primary header | Stream ID = 0x0895, Sequence = 0xC000, length = 25 |
| **6 - 11** | Secondary header | Telemetry for seconds and subseconds |
| **12 - 15** | Padding | cFE pads 0's to consistent bound. |
| **16 - 19** | Temperature | float, little endian |
| **20 - 23** | Humidity | float, little endian |
| **24 - 27** | Sequence Count | uint32, little endian |
| **28 - 31** | Status | uint32, little endian |

# ESP32 Schematic

<img width="1040" height="769" alt="image" src="https://github.com/user-attachments/assets/fb0c6ccd-a4ca-4a68-8260-91c6ac4d451c" />

# How To Build and Run
To make building and running this implementation as easy as possible, I provided two scripts that can be called from the cFS directory that will build or run cFS. 

First, download PlatformIO to upload the ESP32 software inside ESP32_Bridge to the board, or use Arduino IDE. 

``` bash
Navigate to cFS location from this repo,

To build:

./build.sh

To run:

./run.sh
```

If everything is running correctly, the terminal should show cFS posting commands and a GUI should appear. Click 'Enable Telemetry' at destination IP 127.0.0.1 to let data from the TO_LAB app to be transmitted out of cFS to the GUI via UDP.

## DS Log Viewer / FM File Tool

`tools/view_ds_log.py` is a small utility for working with cFS's Data
Storage (DS) log files and File Manager (FM) commands. It has three
subcommands:

| Command       | Needs cFS running? | What it does                                      |
|---------------|---------------------|----------------------------------------------------|
| `decode`      | No                  | Decode a DS-recorded `.dat` file offline           |
| `local-files` | No                  | List `.dat` files sitting in a local directory     |
| `delete-file` | Yes                 | Send `FM_DELETE_FILE` to a live cFS instance       |

Run `python3 tools/view_ds_log.py --help` at any time for the full
option list and examples.

### decode

Reads a raw `.dat` file written by DS and prints each packet in
human-readable form. DS files are just the same CCSDS packets that
flow live over the Software Bus, concatenated after a small file
header — this walks that structure and decodes each packet the same
way the live telemetry GUI does.

```bash
python3 tools/view_ds_log.py decode build-native_std/exe/cpu1/cf/events00001008.dat
```

Optional flags:

```bash
# Only show lines containing specific text
python3 tools/view_ds_log.py decode <file>.dat --filter "high humidity"

# Also write sensor readings out to CSV
python3 tools/view_ds_log.py decode <file>.dat --csv readings.csv
```

### local-files

Lists every `.dat` file in a local directory along with its size —
useful for seeing what DS has accumulated in `/cf` without needing
cFS running at all.

```bash
python3 tools/view_ds_log.py local-files build-native_std/exe/cpu1/cf
```

### delete-file

Sends `FM_DELETE_FILE` to a live cFS instance to remove a specific
onboard file. No confirmation prompt — double-check the path before
sending.

```bash
python3 tools/view_ds_log.py delete-file /cf/sensor_6000_1.dat
```

## Design Information
Here, I wanted to document some design challenges I faced and context for the inner workings of the apps and ESP32 for anyone who wants to trace through my software.

### ESP32 Firmware
- Originally, an STM32 Blackpill was used as the microcontroller, but due to timing issues involving finicky bit-banging to interact with the DHT11 sensor, the LCD was not given enough time to update correctly and collect sensor data on a single core without an RTOS.
- ESP32 has two processor cores, so using FreeRTOS tasks were divided out per core to the main functions of the data collection, LCD update, and packet transmission. Effectively fixed all timing issues and opens up possibilities including WiFi and Bluetooth transmission later on.
  
### ESP32_BRIDGE_APP
- The overall structure for the app was duplicated from the included cFS SAMPLE APP.
- CCSDS packets are received from the ESP32 and are sent to the Software Bus (SB) with ID 0x0895.
- Default serial port is USB0, feel free to change depending on your device.

### LC
- Checks each packet with MID = 0x0895 to see if temperature or humidity is over a certain threshold.
- Strictly edge triggered, so telemetry passing across the threshold will trigger the LC error.
- Currently, LC will change from PASS to FAIL and vice versa immediately upon a threshold crossing, and then will throw an exception if a certain amount of packets are FAIL.
- LC by default is disabled, so for learning purposes I added an LC button to send a command via CI_LAB to set LC_STATE to ACTIVE.
- After a set amount of FAIL packets triggers an exception, LC automatically demotes that specific actionpoint to PASSIVE — the LC app itself stays ACTIVE the entire time. An SC sequence then sends a Set AP State command to re-arm that specific actionpoint back to ACTIVE.
  
### SC
- RTS1 currently starts other commands sequences, follows  NOOP -> Enable RTS3 -> Start RTS3 -> Enable RTS4 
- RTS3 is a self restarting loop that tells SC to evaluate the Action Points (AP) to see whether a threshold for temperature or humidity has been reached. SCH_LAB by itself does not have the functionality to send a payload with its message as LC_SAMPLE_AP_MID needs an 8-byte payload to evaluate Action Points, so RTS3 solves this problem.
- RTS4 rearms LC Action Points to ACTIVE after a fault trigger, since by design LC switches to PASSIVE indefinitely after a fault.

### HS
- Connected to ESP32_BRIDGE_APP to restart app in case there is a crash or a serial disconnection.
- Also connected to cFE apps internally.
  
### DS
- Logs temperature and humidity data to .dat files.
- Included in this repo is a python tool useful for decoding and viewing the data files.
  
### FM
- File manager for cFS that can help manage data files.
- Currently, an included python tool interacts with .dat files to allow a user to manipulate, delete, and manage data files.
  
# Conclusion
Feel free to clone the repo and add your own features to this demo! 
