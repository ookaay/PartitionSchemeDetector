# PartitionSchemeDetector

A tool that talks directly to USB mass storage devices using the raw Bulk-Only Transport (BOT) protocol. It sends SCSI commands over libusb, reads the MBR, and tells you if the partition scheme is MBR, GPT, or unknown.

## What it does

When you plug in a USB drive, the operating system handles all the USB protocol details for you. This program skips the OS layer and talks to the device directly. Here's the flow:

1. Enumerates all USB devices on the system
2. Lets you pick which one to use
3. Finds the mass storage interface (class 0x08, subclass 0x06, protocol 0x50)
4. Sends a CBW (Command Block Wrapper) — the standard USB MSC way to issue commands
5. Reads back a CSW (Command Status Wrapper) to verify
6. Issues a SCSI READ CAPACITY command to get disk size
7. Reads LBA block 0 (the first 512 bytes — the MBR)
8. Checks bytes 0x1FE-0x1FF for the boot signature (0x55 0xAA)
9. If found, checks byte 0x1C2 for the GPT indicator (0xEE)
10. Prints the result: MBR, GPT, or unknown

## Dependencies

You need libusb-1.0. That's it on the USB side. On the C++ side, nothing outside the standard library.

## Building with CMake

The project uses CMake. It finds libusb through pkg-config, but since libusb doesn't always ship a proper CMake config file, the `CMakeLists.txt` includes a fallback that locates it manually.

```cmake
cmake_minimum_required(VERSION 3.10)
project(PartitionSchemeDetector CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try pkg-config first
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBUSB libusb-1.0)
endif()

if(NOT LIBUSB_FOUND)
    # Fallback: try the typical libusb include/library paths
    find_path(LIBUSB_INCLUDE_DIRS
        NAMES libusb.h
        PATHS /usr/include/libusb-1.0
              /usr/local/include/libusb-1.0
              /opt/homebrew/include/libusb-1.0
    )
    find_library(LIBUSB_LIBRARIES
        NAMES usb-1.0 libusb-1.0
        PATHS /usr/lib /usr/local/lib /opt/homebrew/lib
    )
endif()

add_executable(psd app.cpp)
target_include_directories(psd PRIVATE ${LIBUSB_INCLUDE_DIRS})
target_link_libraries(psd PRIVATE ${LIBUSB_LIBRARIES})
```

## Building manually

If you don't want CMake:

```bash
g++ -std=c++17 app.cpp -o psd $(pkg-config --cflags --libs libusb-1.0)
```

Or on Linux without pkg-config:

```bash
g++ -std=c++17 app.cpp -o psd -I/usr/include/libusb-1.0 -lusb-1.0
```

## Running

You need permission to access USB devices. On Linux, run as root or set a udev rule:

```bash
sudo ./psd
```

The program lists all USB devices and asks which one to use:

```
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
```

Pick the mass storage device (usually the one with a recognizable vendor ID). The program does the rest:

```
Ping Successful
DISK SIZE: 119 GB
Partition Scheme: MBR
```

## What's under the hood

The USB mass storage protocol has three layers:

**BOT (Bulk-Only Transport)** — The transport layer. Every command starts with a 31-byte CBW header that includes a tag, data direction, command length, and the actual SCSI command. The device responds with a 13-byte CSW that confirms or rejects the command.

**SCSI** — The command layer. The program uses three commands:
- `TEST UNIT READY` (0x00) — pings the device
- `READ CAPACITY` (0x25) — returns the last LBA and block size
- `READ 10` (0x28) — reads one or more 512-byte blocks

**MBR parsing** — The data layer. Block 0 contains the Master Boot Record. The last two bytes are always 0x55 0xAA for a valid MBR. Byte 0x1C2 tells you if it's protective GPT (0xEE) or regular MBR.

## Limitations

- This is a diagnostic tool, not a full USB stack. It handles one specific class of devices (mass storage with BOT).
- The program needs to claim the interface, which means the kernel driver gets detached. The USB device won't be accessible as a normal drive while this runs.
- Only works with devices that expose the mass storage interface. Keyboards, mice, and other HID devices won't work.
- It uses `libusb_set_auto_detach_kernel_driver` but some systems may need manual driver detachment.

## Why this exists

I wanted to understand how USB storage actually works below the filesystem layer. Most developers work with files — directories, read(), write(). But somewhere between your application and the spinning rust, there's a wire-level protocol where 31-byte headers and SCSI CDBs are the only way to move data. This program makes that layer visible.
