# Gzip Compression Guide

## Overview

This project uses gzip compression to reduce the size of files served from the ESP32's SPIFFS filesystem. This significantly reduces:
- **Flash storage usage** (up to 70-80% for text files)
- **Network transfer time** (faster page loads)
- **Memory usage** (smaller files to stream)

## How It Works

### Build Process

1. **Webpack** compiles React applications
2. **Grunt** copies files to `data/web/`
3. **Gzip task** compresses `.js`, `.css`, `.html` files
4. Original files are deleted, only `.gz` versions remain

### ESP32 Serving

When a client requests a file (e.g., `/web/game/index.html`):

1. **First attempt**: Check if `index.html` exists
   - If yes → serve it normally
   
2. **Second attempt**: Check if `index.html.gz` exists
   - If yes → serve it with `Content-Encoding: gzip` header
   
3. **Not found**: Return 404 error

The browser automatically decompresses gzipped content when it sees the `Content-Encoding: gzip` header.

## File Size Comparison

Typical compression ratios:

| File Type | Original Size | Gzipped Size | Savings |
|-----------|--------------|--------------|---------|
| HTML      | 10 KB        | 2-3 KB       | 70-80%  |
| CSS       | 20 KB        | 4-5 KB       | 75-80%  |
| JavaScript| 100 KB       | 25-30 KB     | 70-75%  |

**Example:**
```
bundle.js       245 KB
bundle.js.gz     62 KB  (74% reduction)
```

## Usage

### Automatic Compression (Recommended)

The build process automatically compresses files:

```bash
npm run build
```

This runs:
1. `clean` - Remove old builds
2. `webpack:build` - Compile React apps
3. `copy` - Copy to data/web/
4. `gzip` - Compress files ← Automatic!

### Manual Compression

Compress files separately:

```bash
npm run gzip
```

Or with Grunt:

```bash
grunt gzip
```

## Implementation Details

### Gruntfile.js

The custom gzip task:

```javascript
grunt.registerTask('gzip', function() {
  // Recursively find all .js, .css, .html files
  // For each file:
  //   1. Read file content
  //   2. Compress with zlib.gzip()
  //   3. Write as filename.gz
  //   4. Delete original file
});
```

### DeviceWebserver.cpp

The file serving logic:

```cpp
bool DeviceWebserver::handleFileRead(String path) {
    // Try original file first
    if (SPIFFS.exists(path)) {
        // Serve normally
    }
    // Try .gz version
    else if (SPIFFS.exists(path + ".gz")) {
        _server->sendHeader("Content-Encoding", "gzip");
        // Serve gzipped file
    }
    // Not found
    else {
        return false;
    }
}
```

## Serial Monitor Output

When serving gzipped files, you'll see:

```
handleFileRead: /web/game/index.html
Found gzipped file: /web/game/index.html.gz
Serving gzipped content with Content-Encoding: gzip
File sent: /web/game/index.html.gz (Content-Type: text/html)
```

## Browser Behavior

Modern browsers automatically:
1. Send `Accept-Encoding: gzip` header
2. Receive gzipped content
3. Decompress transparently
4. Render the page normally

**No special client-side code needed!**

## Troubleshooting

### Files not compressing

Check that files exist in `data/web/`:
```bash
ls -la data/web/game/
ls -la data/web/system/
```

### Gzip task fails

Ensure you've run the build first:
```bash
npm run build
```

### Browser shows garbled content

Check that `Content-Encoding: gzip` header is set:
- Open browser DevTools → Network tab
- Click on the file
- Check Response Headers

### SPIFFS upload fails

Verify partition size in `platformio.ini`:
```ini
board_build.partitions = app3M_spiffs9M_fact512k_16MB.csv
```

## Benefits

✅ **Reduced SPIFFS usage** - Store more files  
✅ **Faster page loads** - Less data to transfer  
✅ **Lower memory usage** - Smaller files to stream  
✅ **Transparent to client** - Browser handles decompression  
✅ **Automatic fallback** - Serves uncompressed if .gz not found  

## Best Practices

1. **Always run build** before uploading filesystem
2. **Check serial monitor** to verify gzipped files are served
3. **Test in browser** to ensure proper decompression
4. **Monitor SPIFFS usage** to ensure enough space

## Advanced: Selective Compression

To skip compression for certain files, modify `Gruntfile.js`:

```javascript
// Skip files matching pattern
if (filename.includes('no-compress')) {
  return; // Skip this file
}
```

## Performance Impact

**ESP32 CPU Usage:**
- Serving gzipped files: ~Same as uncompressed
- No runtime compression needed (pre-compressed)

**Network Transfer:**
- 70-80% reduction in transfer time
- Especially beneficial on slow WiFi connections

