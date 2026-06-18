# PartitionSchemeDetector

Talks directly to USB mass storage devices using the raw Bulk-Only Transport protocol. No OS filesystem layer — just SCSI commands over libusb, raw MBR parsing, and a result.

## Quick start

```bash
# Install libusb if you don't have it
apt install libusb-1.0-0-dev    # Debian/Ubuntu
brew install libusb              # macOS

# Build
cmake -B build && cmake --build build
# Or without cmake:
g++ -std=c++17 app.cpp -o psd $(pkg-config --cflags --libs libusb-1.0)

# Run (needs root for USB access)
sudo ./psd
```

## How it looks

```
$ sudo ./psd

-------------------
STARTING
-------------------

Which of these device do you want to use? [1 - 7]
1)  BUS: 001 PORT: 001 ID 1d6b:0002
2)  BUS: 001 PORT: 002 ID 0424:2512
3)  BUS: 001 PORT: 003 ID 0424:2740
4)  BUS: 002 PORT: 001 ID 0424:2744
5)  BUS: 003 PORT: 002 ID 0781:5583
6)  BUS: 003 PORT: 003 ID 0bda:5401
7)  BUS: 003 PORT: 004 ID 13d3:5652
Your choice: 5

Ping Successful
DISK SIZE: 119 GB
Partition Scheme: MBR
```

Pick the mass storage device (usually obvious from the vendor ID). The program sends a SCSI TEST UNIT READY to ping it, reads the capacity, grabs block 0, and checks the MBR boot signature + partition type byte.

## What you need

- libusb-1.0 (development headers)
- C++17 compiler
- CMake (optional — can build manually)

### Manual build without CMake

```bash
g++ -std=c++17 app.cpp -o psd $(pkg-config --cflags --libs libusb-1.0)
```

If your system doesn't have pkg-config:

```bash
g++ -std=c++17 app.cpp -o psd -I/usr/include/libusb-1.0 -lusb-1.0
```

## Architecture

Three layers, bottom to top:

**BOT transport** — Every command starts with a 31-byte Command Block Wrapper (CBW) that packs a tag, data direction, command length, and the SCSI command itself. The device replies with a 13-byte Command Status Wrapper (CSW) saying pass/fail/phase error.

**SCSI commands** — Three commands used:
- `TEST UNIT READY` (0x00) — checks the device is alive
- `READ CAPACITY` (0x25) — gets the last LBA and block size
- `READ 10` (0x28) — reads 512-byte blocks from the device

**MBR parsing** — Block 0 is the MBR. Bytes 0x1FE-0x1FF are always `55 AA` for a valid MBR. Byte 0x1C2 tells you if it's protective GPT (`0xEE`) or regular MBR.

## Limitations

- Only handles mass storage devices with BOT. Won't work on HID devices.
- Detaches the kernel driver while running — the device won't appear as a normal drive during execution.
- Some systems may need manual driver detachment instead of the auto-detach call.
