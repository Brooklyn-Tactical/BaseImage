# Release #3
## Versions
```versions
Firmware: v2.1
Filesystem: v1.3
```
For hardware versions:
```for_hardware
v1.0
```

## Release Notes
Better descriptions in Game UI (with multilingual translations). Fixes to layout in System UI. 
Updated library dependencies to latest versions (mostly for HFSM's StateChangeListener).

## Instructions
```bash
pip install esptool
python -m esptool --chip esp8266 write-flash 0x0 ./firmware.bin  2097152 ./littlefs.bin
```
