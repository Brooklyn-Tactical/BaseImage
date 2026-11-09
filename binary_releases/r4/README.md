# Release #4
## Versions
```versions
Firmware: v2.2
Filesystem: v1.4
```
For hardware versions:
```for_hardware
v1.0
```

## Release Notes

* Added prop demo mode to the admin interface

## Instructions
```bash
python -m esptool --chip esp8266 write-flash 0x0 ./firmware.bin  2097152 ./littlefs.bin
```

