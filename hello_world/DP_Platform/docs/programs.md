# Description and download of programs used during the project design process

## Arduino_soft
System Overview: Multi-module stepper motor controller with optional IMU (MPU6500) on board ID 1. Each board gets a unique ID via ADC voltage divider, determining motor count (3 motors for ID 1, 4 for others) and whether MPU6500 is enabled.

### main.cpp - Boot & Main Loop

`setup()`: Reads board ID, initializes stepper motors. If ID=1, also initializes MPU6500.

`loop()`: Continuously updates stepper motors. If ID=1, reads MPU6500 every 10ms. Receives UART frames and processes commands if addressed to this board (rx.id matches myID or is broadcast 0).

### board_id.cpp - Hardware Configuration

`readBoardID()`: Reads ADC value with 32 samples averaged. Different voltage thresholds map to board IDs 1-5.

`getMotorCount()`: Returns 3 motors for ID 1 (MPU present), 4 for all others.

### stepper_ctrl.cpp - Motor Control
Four stepper motors (stepper1-4) configured in HALF4WIRE mode. Motor 4 disabled on ID 1 (pin conflicts with MPU6500).

**Key functions:**

- `getMotor()`: Retrieves motor pointer by ID (1-indexed).

- `initStepper()`: Sets up GPIO, registers motors, configures speed/acceleration.

- `updateStepper()`: Main update called every loop. Runs motors in either position mode (`run()`) or velocity mode (`runSpeed()`).

- Movement: `moveStepper()` (absolute), `moveStepperRelative()` (relative), `setStepperVelocity()` (constant speed).

- Configuration: `setStepperSpeed()`, `setStepperAcceleration()`.

- Status: `getStepperPosition()`, `isStepperBusy()`, `homeStepper()` (move to 0).

- `normalizeStepperPosition()`: Wraps position to single revolution using modulo.

All functions support motorID=0 to broadcast to all motors.

### mpu6500.cpp - IMU Sensor
Only active on ID 1. Uses SPI communication (CS on pin 10).

**Data arrays track current and peak values:**

- Accelerometer: pitchCurrent/Peak, rollCurrent/Peak, maxDynamicG
- Gyroscope: gxCurrent/Peak, gyCurrent/Peak, gzCurrent/Peak
- pushCount: Increments when dynamic G exceeds 0.10g

**`readMPU6500():`**

- Reads 6 accelerometer bytes from register 0x3B, converts to 16-bit signed integers, divides by 16384 to get gravitational units (g).
- Calculates pitch/roll angles using atan2(), converts to degrees.
- Computes total acceleration magnitude and dynamic G (deviation from 1g).
- Tracks peak values; increments push counter if motion detected.
- Reads 6 gyroscope bytes from register 0x43, converts using 131 LSB/°/s, tracks peaks.

### process_commands.cpp - Command Dispatcher

`processFrame()` receives UART frames and routes to handlers based on command type.

**Determines:**

- broadcast: true if frame ID is 0 (for all devices).
- replyAllowed: false for broadcasts, true for unicast (prevents reply flooding).

**Command handlers:**

- System: CMD_PING, CMD_STATUS (position, push count, motor busy status)
- IMU (ID 1 only): CMD_LIVE_MPU (current pitch/roll), CMD_LIVE_GYRO (current rates), CMD_HIST_MPU (peaks), CMD_HIST_GYRO (rate peaks), CMD_PUSH_INFO, CMD_RESET_STAT
- Motor: CMD_HOMING, CMD_MOVE_ABS, CMD_MOVE_REL, CMD_MOVE_VELOCITY, CMD_STOP_MOTOR, CMD_SET_SPEED, CMD_SET_ACCEL

Each handler processes the command and optionally sends a response frame via `sendFrame()`.

### uart_protocol.cpp - Communication Protocol

UART frame structure: ID, Command, MotorID, Data1, Data2, Data3, Data4. 

Broadcast (ID=0) commands reach all devices without requiring responses.

### uart_gui.py - Control GUI

!!! note "Using the application"
    For a single aNano only.

Python GUI for controlling stepper motors via UART. Tkinter-based with COM port selection and 115200 baud rate.

**UI Sections:**

- Header: COM port selector, connect/disconnect, status indicator
- Target: Board ID (0-5), Motor ID (0-5)
- Jog: << -1000, < -100, > +100, >> +1000 steps; HOME, ZERO buttons
- Positioning: Absolute/relative move inputs with MOVE buttons
- Motion Profile: Speed and acceleration setters
- Velocity Mode: START CW/CCW, STOP for continuous rotation

**Key Methods:**

- `connect()`: Opens serial port, starts background RX thread
- `rx_worker()`: Thread that reads serial data continuously, updates RX log
- `send_frame()`: Packs command into binary frame with format: preamble (0xAA), board ID, command (16-bit), motor ID, data1, padding, CRC
- `calc_crc()`: XOR checksum of all frame bytes
- Command helpers: `cmd_home()`, `cmd_stop()`, `cmd_zero()`, `move_rel()`, `cmd_vel_cw()`, `cmd_vel_ccw()`
- Logging: Split TX/RX ScrolledText areas display sent commands and received messages.

!!! note "To be checked"
    Check whether aNano responds after a frame is sent from the application.

## SPMapp

### spm_manipulator.py

- Tkinter GUI for a coaxial SPM inverse kinematics demo.
- Lets you set mechanism geometry: L1, L2, platform radius, platform height, and L1 tilt.
- Provides roll/pitch/yaw sliders to pose the moving platform.
- Computes inverse kinematics for 3 actuator angles (Theta 1..3) using the platform pose.

**Draws a 3D visualization with:**

- base circle,
- active arms L1,
- passive arms L2 as curved links,
- moving platform triangle,
- orientation normal vector.

### uart_spm.py

- More advanced HMI around the same SPM kinematics.
- Adds serial/UART support with COM port and baud rate selection.
- Can connect to a real MCU or simulate responses if no serial port is available.

**Supports commands:**

- $GEO send geometry,
- $ORI send orientation,
- $MOV send manual motor positions,
- $HOME, $STOP, $ESTOP.
- Displays two log panes: RX from MCU and TX commands from PC.
- Parses MCU messages like $POS, $RPY, $STS, $INFO.
- Updates the 3D kinematics preview and status panel from actual or simulated MCU state.

**Key difference:**

- spm_manipulator.py is a standalone kinematics visualization tool.
- uart_spm.py is a controller HMI with communication, logging, status, and MCU interaction.

!!! info "Usability"
    The HMI app (uart_spm.py) allows for testing a separate manipulator module in the future, prior to its integration into the main system.

## Esp_UART_bridge
This is an ESP32-based UART bridge plus a Python GUI for communicating with an “aNano” motion controller.

!!! info "Usability"
    This ESP32 bridge program allows for the independent control of stepper motors in the aNano system via the downloadable *Esp_UART_bridge* application (which includes *uart_gui*). Unlike the program found in the *Arduino_soft* file — which requires an Arduino to be connected for control—this setup connects directly to the ESP32 bridge. It is useful for testing the thruster control system in isolation prior to its full integration into the main system.

### main.c
- Configures UART_NUM_2 for the external device (ANANO_UART) using GPIO 17 as TX and GPIO 16 as RX.
- Installs a UART driver with 2048-byte RX/TX buffers.

**Starts two FreeRTOS tasks:**

- pc_to_anano_task: reads bytes from UART_NUM_0 (PC/serial console) and forwards them to UART_NUM_2.
- anano_to_pc_task: reads bytes from UART_NUM_2 and forwards them to UART_NUM_0.

So the ESP32 acts as a simple bidirectional serial bridge between the PC and the aNano device.

### uart_gui.py
- Builds a Tkinter GUI for sending framed commands to the device over a serial COM port.
- Defines commands like CMD_HOMING, CMD_MOVE_ABS, CMD_SET_SPEED, telemetry commands, and status queries.

**Creates a frame format:**
PREAMBLE (0xAA)
board_id
command
motor_id
data1..data4
crc

Uses XOR-based CRC over the frame fields.

Has:
- Connect/disconnect logic
- RX worker thread parsing incoming frames
- UI controls for move, speed, velocity, stop, home, zero
- Telemetry polling thread

### **How they work together**
uart_gui.py sends binary frames over a PC serial port.

The ESP32 bridge in main.c forwards those bytes to the external ANANO_UART.

Responses from the device come back via UART_NUM_2 and are forwarded to the PC port for the GUI to parse.

**Key points:**

- UART_NUM_0 is the ESP32’s USB/console UART.
- UART_NUM_2 connects to the external hardware.
- There is no protocol parsing in main.c; it just forwards raw bytes.
- The Python GUI handles framing, CRC, and display of telemetry/status.

!!! note "To be checked"
    Check whether aNano responds after a frame is sent from the application.

## System Summary

### Arduino_soft
Arduino_soft is the main aNano firmware responsible for controlling multiple stepper motors and handling communication through a UART protocol. Each board automatically determines its ID using an ADC voltage divider, which defines the number of available motors and whether the MPU6500 IMU is enabled. The firmware supports motor positioning, velocity control, homing, speed and acceleration configuration, status monitoring, and IMU telemetry, while processing both unicast and broadcast commands from external applications.

### SPMapp
SPMapp is a software suite for developing and testing a coaxial Spherical Parallel Manipulator (SPM). The spm_manipulator.py application provides inverse kinematics calculations and a 3D visualization of the mechanism, while uart_spm.py extends these capabilities with a full HMI, serial communication, logging, simulation mode, and MCU interaction. This allows the manipulator concept to be validated and tested independently before integration with the main aNano system.

### Esp_UART_bridge
Esp_UART_bridge is an ESP32-based communication bridge that enables a PC application to communicate directly with the aNano controller. The ESP32 transparently forwards data between its USB serial interface and the external UART connected to aNano, without interpreting or modifying the protocol. Combined with the accompanying GUI, it provides independent testing of motor control, telemetry, and communication functions before full system integration.

!!! info Verification Required

    - GUI sends valid frame.
    - ESP32 forwards frame correctly.
    - aNano receives and processes it.
    - CRC passes.
    - processFrame() executes.
    - aNano sends a response.
    - ESP32 forwards response to PC.
    - GUI receives and displays the reply.

**Primary check:** Verify that aNano responds to valid unicast frames sent from the application.

## **Download programs**
|Name | Download|
| :--- | :--- |
| Arduino_soft.zip | [Download 📥](Program_down_files\Arduino_soft.zip) |
|  SPMapp.zip | [Download 📥](Program_down_files\SPMapp.zip) |
| Esp_UART_bridge.zip | [Download 📥](Program_down_files\Esp_UART_bridge.zip) |