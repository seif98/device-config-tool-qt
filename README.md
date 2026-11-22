# Device Config Tool (Qt 6 / C++17)

A desktop tool to read, edit and write the parameters of an embedded device over a
framed binary protocol. It runs **without hardware** using a built-in device simulator,
and talks to a real device over a serial port.

<!-- TODO: add screenshot -->

## Features
- Parameter table with range validation, per-row status and confirmed values from the device
- Read all / Write all / Reset to defaults
- Import and export settings as JSON
- Live telemetry display and a timestamped log
- Pluggable transports: simulator, serial port (QtSerialPort); TCP can be added the same way
- Framed protocol with CRC-16 and automatic resynchronisation after noise
- GUI-free core library with unit tests (run under AddressSanitizer in development)

## Protocol
`[0xA5][LEN][CMD][PAYLOAD x LEN][CRC16 lo][CRC16 hi]` - CRC-16/CCITT-FALSE over LEN, CMD, PAYLOAD.

| Cmd | Name      | Payload                         |
|-----|-----------|---------------------------------|
| 01  | Get       | id                              |
| 02  | Set       | id, value (int32 LE)            |
| 81  | Value     | id, value (reply to Get/Set)    |
| 7F  | Error     | id, code                        |
| 90  | Telemetry | seq (u32 LE), reading (i32 LE)  |

## Build
Requires Qt 6 (Widgets, SerialPort) and CMake 3.16+. Open `CMakeLists.txt` in Qt Creator, or:

    cmake -S . -B build -DCMAKE_PREFIX_PATH=<path-to-Qt6>
    cmake --build build
    ctest --test-dir build

## Structure
- `core/` - protocol, parameter table, device simulator (no Qt)
- `src/` - Qt application: transports, device client, main window
- `tests/` - unit tests for the core
