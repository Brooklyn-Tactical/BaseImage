# Release #1
## Versions
```versions
Firmware: v2.0
Filesystem: v1.0
```
For hardware versions:
```for_hardware
v1.0
```

## Release Notes
Initial software release for the Bio-vial prop.

## Instructions
```bash
python -m esptool --chip esp8266 write-flash 0x0 ./firmware.bin  2097152 ./littlefs.bin
```
