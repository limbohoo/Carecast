Carecast Weather Sonification: System Documentation
1. Project Overview
Carecast is an advanced IoT weather station that translates meteorological data into soundscapes. Unlike traditional weather stations that simply display numbers, Carecast uses an ESP32-S3-R8 high-performance controller to drive a circular LCD and play high-fidelity environmental audio (WAV) based on real-time weather conditions from local sensors or OpenWeatherMap API.

The system utilizes a unique Dual-Board Stack Architecture, separating high-performance computing (UI/Audio) from power management and base interfaces.

2. Development Journey & Challenges
The development process involved solving critical hardware integration and software concurrency challenges:

The "Dual Brain" Integration: The system uses two ESP32 chips. The challenge was identifying that the audio DAC (PCM5100A) is physically shared between the Base Board (Chip A) and the Main Board (Chip B) via an analog switch (CH445P).

Solution: Implemented GPIO control (Pin 0) to "steal" the DAC control from the base board to the S3 main board.

Audio Format & Storage: Initial attempts using MP3 files resulted in static noise because the PCM5100A DAC requires raw PCM data.

Solution: Standardized all audio assets to 16-bit, 44.1kHz WAV format and implemented a 1-bit SDMMC driver for high-speed reading.

The Blocking UI Problem: The UI initially froze during audio playback because the file reading loop blocked the main CPU thread.

Solution: Migrated to FreeRTOS. The audio engine now runs on a separate CPU core (Core 0) as a high-priority task, allowing the LVGL UI to render smoothly on the main loop (Core 1).

Signal Routing: Muting and unmuting the amplifier required coordinating GPIO states between the S3 and the Base board to prevent hardware silence.

3. System Logic Diagram
This diagram illustrates how data flows from the internet/sensors to the user's eyes and ears.
https://encrypted-tbn2.gstatic.com/licensed-image?q=tbn:ANd9GcQ7W_yZKdKQwDAH4uAoUYuVA6wWXlUZsG_2d50nZIwIi9zyp2ljrBw-lkFh1OjsiucqeOnfNal1wYgNV4ENXEAXD_JAGdbiWommpxqTg_REw7HcKdc<img width="3999" height="3039" alt="image" src="https://github.com/user-attachments/assets/55448a5a-ca02-4e71-a32d-94e335f41709" />

4. Hardware Mapping (GPIO Documentation)
The system consists of two stacked boards. Below is the pinout configuration for the ESP32-S3-R8 (Top Board) which controls the system.

Audio & Storage Interface
<img width="495" height="317" alt="Screenshot 2025-12-10 at 22 58 06" src="https://github.com/user-attachments/assets/1fd3556c-34fc-4122-aa2c-5f384a53c24b" />

Display & User Interface
<img width="449" height="389" alt="Screenshot 2025-12-10 at 22 58 25" src="https://github.com/user-attachments/assets/b2c29feb-c7d2-455d-ae8e-a5751a06d0c9" />

5. Software File Structure & Functions
The codebase is modularized to separate hardware drivers from business logic.

1. Carecast_Main.ino (The Orchestrator)

Role: Entry point.

Functions:

Initializes Hardware (SD, WiFi, Display, Audio).

loop(): Runs the LVGL timer and Event Manager.

Handles Serial debugging commands (t, w, f).

2. audio_manager.cpp / .h (The Engine)

Role: Handles sound hardware and the FreeRTOS task.

Key Features:

audioTask(): A dedicated FreeRTOS task running on Core 0. It waits for file paths to arrive in a Queue, then reads the SD card and pushes bytes to I2S.

audio_manager_init(): Sets up I2S standard driver and flips GPIO 0 (Switch) and GPIO 38 (Mute).

3. event_manager.cpp / .h (The Logic Glue)

Role: Connects user actions to system responses.

Functions:

Decouples the Knob from the Audio.

When EVT_TOUCH_CLICK happens -> calls audio_play_manual_current().

When EVT_TIMER_30MIN fires -> checks if audio is playing, then calls audio_play_ambient_forecast().

4. scr_st77916.c & lv_conf.h (The Visuals)

Role: Display drivers.

Functions:

Configures the ST77916 display controller.

Flushes the LVGL draw buffer to the QSPI screen.

5. bidi_switch_knob.c (The Input)

Role: Input driver.

Functions:

Detects rotation and clicks.

Uses a callback system to notify the event_manager of user inputs.

6. Implementation Guide (How to run)
Prepare SD Card:

Format to FAT32.

Create folder /audio.

Convert all sound files to WAV (16-bit, 44100Hz).

Hardware Setup:

Stack the ESP32-S3 board onto the Base board.

Connect USB to the Base board (ensure USB CDC On Boot is Enabled in Arduino IDE if using S3 native USB, or standard setting if using Base UART).

Flash Settings (Arduino IDE):

Board: ESP32S3 Dev Module.

PSRAM: OPI PSRAM (Essential for R8 chip).

Flash Mode: QIO 80MHz.

Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS) - Recommended for large codes.
