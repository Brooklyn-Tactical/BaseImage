# Release #2
## Versions
```versions
Firmware: v2.0
Filesystem: v1.1
```
For hardware versions:
```for_hardware
v1.0
```

## Release Notes
Upgrades web UI to be multilingual. 

## Instructions
```bash
pip install esptool
python -m esptool --chip esp8266 write-flash 0x0 ./firmware.bin  2097152 ./littlefs.bin
```
