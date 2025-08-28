# LogicData SmartDesk Controller

An ESP-IDF port of the LogicData protocol implementation that acts as a man-in-the-middle interface between your standing desk's control buttons and the table controller. This project allows you to intercept desk height information and add smart automation features to your LogicData-compatible standing desk.

## Overview

This project implements a passive listener that intercepts the serial communication between the desk controller and handset, allowing you to:

- **Monitor desk height** in real-time
- **Set height presets** by pressing both buttons simultaneously
- **Automatic positioning** with double-click to saved heights
- **Manual control** with long-press pass-through to original desk functions

> [!CAUTION]
> This interface may violate your LOGICDATA warranty.

**Use at your own risk!**

## Hardware Requirements

- Any ESP development board, I used ESP32
- Logic level shifter (3.3V to 5V) for reliable operation
- Breadboard or PCB for connections
- Dupont wires or appropriate connectors

## Pin Connections

### ESP32 GPIO Assignments (can be modified in the code)

| Function          | ESP32 GPIO | Description                       |
| ----------------- | ---------- | --------------------------------- |
| LogicData RX      | GPIO 25    | Serial data from desk controller  |
| Desk UP Control   | GPIO 27    | UP signal to desk controller      |
| Desk DOWN Control | GPIO 26    | DOWN signal to desk controller    |
| User Button UP    | GPIO 21    | Physical button for UP commands   |
| User Button DOWN  | GPIO 19    | Physical button for DOWN commands |

### Connection Diagram

```
[Control Buttons] ←→ [ESP32] ←→ [Desk Controller]
```

### Desk Controller Side (to table controller)

Controller pinout is shown in the picture below

![Controller pinout](/images/controller-pinout.png)

| LOGICDATA Pin  | ESP32 GPIO | Description                          |
| -------------- | ---------- | ------------------------------------ |
|       5V       | VIN/5V     | 5V Power Supply                      |
|       GND      | GND        | Ground                               |
|       UP       | GPIO 27    | UP control signal                    |
|       DOWN     | GPIO 26    | DOWN control signal                  |
|       Serial   | GPIO 25    | Serial communication (receive only)  |
|       GND      | GND        | Ground (optional, additional ground) |

### Control Buttons Side (from original handset)

Wire the original control buttons or create your own button interface:

| Button Function | ESP32 GPIO | Description                         |
| --------------- | ---------- | ----------------------------------- |
| UP Button       | GPIO 21    | Connect button between GPIO and GND |
| DOWN Button     | GPIO 19    | Connect button between GPIO and GND |
| 5V              | VIN/5V     | Power supply for ESP32              |
| GND             | GND        | Ground connection                   |

## Features

### Button Functions

- **Long Press UP/DOWN**: Direct control of desk movement (pass-through)
- **Double-click UP**: Move to saved high position
- **Double-click DOWN**: Move to saved low position
- **Press both UP+DOWN simultaneously**: Save current height as preset

### Smart Functions

- Real-time height monitoring and logging
- Automatic height presets (learns your preferred positions)
- Smooth automatic positioning to saved heights
- **Non-volatile preset storage (NVS)** - preset heights are automatically saved and persist across power cycles
- **Automatic preset management** - presets are updated and saved whenever you set new positions

## NVS Storage

The SmartDesk controller uses ESP32's Non-Volatile Storage (NVS) to persistently store preset heights:

- **Automatic Saving**: Preset heights are automatically saved to NVS whenever they are updated
- **Persistent Storage**: Presets survive power cycles, firmware updates, and system resets
- **Namespace**: Uses "smartdesk" namespace for organized storage
- **Keys**: Stores "low_height" and "high_height" as 8-bit values
- **Error Handling**: Graceful fallback to defaults if NVS operations fail

## Protocol Information

The LOGICDATA protocol operates at:

- **Speed**: 1000 bps (1 bit per millisecond)
- **Voltage**: 5V logic levels
- **Format**: 32-bit words with specific framing
- **Idle state**: Logic HIGH (5V)
- **Data encoding**: Inverted Manchester encoding

For detailed protocol analysis, see the [technical blog post](https://technicallycompetent.com/blog/hacking-logicdata-desk/).

## Installation

1. **Clone and build**:

   ```bash
   git clone <this-repository>
   cd LogicData_SmartDesk
   idf.py set-target <your chip type>
   idf.py build
   ```

2. **Flash to ESP32**:

   ```bash
   idf.py flash monitor
   ```

3. **Wire connections** according to the pin diagram above

4. **Test functionality** by observing serial output during desk operation

## Voltage Level Issues

The table operates at 5V, pin controlling is okay, but pin for receiving data from the table controller is under stress, because Espressif SoCs are meant to be operated on 3.3V logic.

## Credits and References

This project is an ESP-IDF port of the excellent work by:

- **Original Arduino Implementation**: [@phord/RoboDesk](https://github.com/phord/RoboDesk) - Arduino code for LOGICDATA protocol
- **Protocol Analysis**: [Hacking the Logicdata Desk Controller](https://technicallycompetent.com/blog/hacking-logicdata-desk/) - Detailed reverse engineering blog post

Major differences from the original:

- Ported from Arduino to ESP-IDF framework
- Added preset height functionality
- Implemented automatic positioning system
- Enhanced button control with multiple gesture support

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.
