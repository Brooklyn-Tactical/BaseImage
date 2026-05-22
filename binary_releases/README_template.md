# Release #1
## Versions
```versions
Firmware: {{firmware_version}}
Filesystem: {{filesystem_version}}
```
For hardware versions:
```for_hardware
{{hardware_version_list}}
```

## Release Notes

{{release_notes}}

## Instructions
```bash
python -m esptool --chip esp8266 write-flash 0x0 ./firmware.bin  2097152 ./littlefs.bin
```

