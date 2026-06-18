# 6-DOF IR Tracker

A 6 degrees-of-freedom head tracker using an IR camera, IR LEDs, 
and a BNO085 IMU. Sends fused position and orientation data over 
UDP to Unreal Engine 5.

## How it works
1. ESP32 reads Roll, Pitch, Yaw and acceleration from BNO085 IMU over I²C at 100Hz
2. Python bridge reads IMU data over Serial and IR LED positions from camera
3. solvePnP extracts XYZ position from 3 IR LED points
4. Fused packet sent over UDP to UE5 at ~60Hz

## Hardware
- ESP32
- BNO085 IMU (I²C, address 0x4B)
- IR camera (Camera Index 0)
- 3x IR LEDs in triangular constellation (40mm top, ±30mm base)

## Wiring
| BNO085 Pin | ESP32 Pin |
|------------|-----------|
| VDD        | 3.3V      |
| GND        | GND       |
| SDA        | GPIO21    |
| SCL        | GPIO22    |
| RST        | GPIO4     |
| ADR        | 3.3V      |
| PS0        | GND       |
| PS1        | GND       |

## UDP Packet Format (JSON to UE5)
```json
{
  "tracking": true,
  "x": 0.0, "y": 0.0, "z": 30.0,
  "roll": 0.0, "pitch": 0.0, "yaw": 0.0,
  "accX": 0.0, "accY": 0.0, "accZ": 9.8,
  "leds": [{"x":0,"y":0,"z":0}, ...],
  "ts": 1234567890.0
}
```
- Position in UE5 cm (converted from camera mm)
- Orientation in degrees from IMU
- Default port: 12345

## Setup

### Firmware
1. Install SparkFun BNO08x library in Arduino IDE
2. Open `firmware/tracker_imu/tracker_imu.ino`
3. Flash to ESP32

### Python Bridge
```bash
pip install opencv-python numpy pyserial
python bridge/tracker_udp_bridge.py
```
Edit `SERIAL_PORT` and `UE5_IP` in the script to match your setup.

Run in demo mode (no hardware needed):
```bash
python bridge/tracker_udp_bridge.py --demo
```

## Dependencies
- [SparkFun BNO08x Arduino Library](https://github.com/sparkfun/SparkFun_BNO08x_Arduino_Library)
- Python: opencv-python, numpy, pyserial