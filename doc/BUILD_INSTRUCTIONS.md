# Build Instructions

## Prerequisites

1. **Node.js** (v16 or higher)
2. **PlatformIO** (for ESP32 development)
3. **npm** (comes with Node.js)

## Quick Start

### 1. Install Dependencies

```bash
npm install
```

This will install:
- React 18
- Webpack 5
- Babel
- GruntJS
- All required loaders and plugins

### 2. Build React Applications

```bash
npm run build
```

This command will:
1. Clean previous builds (`dist/` and `data/web/`)
2. Compile React apps using Webpack
3. Copy compiled files to `data/web/game/` and `data/web/system/`
4. Copy all CSS, images, and other assets
5. **Gzip compress** all `.js`, `.css`, and `.html` files
6. Delete original uncompressed files (only `.gz` versions remain)

### 3. Upload to ESP32

**Upload filesystem (SPIFFS):**
```bash
pio run --target uploadfs
```

**Upload firmware:**
```bash
pio run --target upload
```

**Monitor serial output:**
```bash
pio device monitor
```

## Development Workflow

### Development Proxy Server (Recommended for Active Development)

The development proxy server allows you to develop React applications locally without uploading to SPIFFS on every change.

**Start the proxy server:**
```bash
npm run proxy -- <port> <apiPath> <esp32Host>
```

**Examples:**
```bash
# Using mDNS hostname
npm run proxy -- 8080 /api myESP32.local

# Using IP address
npm run proxy -- 8080 /api 192.168.4.1

# Using different port
npm run proxy -- 3000 /api 192.168.4.1
```

**Parameters:**
- `port` - Local port number for the proxy server (e.g., `8080`, `3000`)
- `apiPath` - API root path that will be forwarded to ESP32 (e.g., `/api`)
- `esp32Host` - ESP32 hostname or IP address (supports mDNS `.local` names, FQDNs, or IP addresses)

**How it works:**

1. **Static File Serving** - Non-API requests are served from local `/dist` directory:
   - `http://localhost:8080/` → serves `/dist/index.html`
   - `http://localhost:8080/game/` → serves `/dist/game/index.html`
   - `http://localhost:8080/game/bundle.js` → serves `/dist/game/bundle.js`
   - `http://localhost:8080/system/` → serves `/dist/system/index.html`

2. **API Proxying** - Requests starting with `/api` are transparently forwarded to ESP32:
   - `http://localhost:8080/api/game/state` → `http://myESP32.local/api/game/state`
   - `http://localhost:8080/api/system/info` → `http://myESP32.local/api/system/info`
   - All HTTP methods (GET, POST, PUT, DELETE) are supported
   - All headers, cookies, and request/response bodies are passed through unchanged
   - Response status codes and headers are preserved

**Development workflow with proxy:**

1. Make sure ESP32 is running and accessible (connect to `ESP32-WebServer` WiFi)
2. Build React apps: `npm run build` (creates files in `/dist`)
3. Start proxy server: `npm run proxy -- 8080 /api 192.168.4.1`
4. Open browser: `http://localhost:8080/game/` or `http://localhost:8080/system/`
5. Edit React components in `web/game/src/` or `web/system/src/`
6. Rebuild: `npm run build` (or use watch mode - see below)
7. Refresh browser to see changes (no SPIFFS upload needed!)

**Benefits:**
- ✅ No SPIFFS upload required for UI changes
- ✅ Faster development iteration (seconds vs minutes)
- ✅ Test against real ESP32 APIs and hardware
- ✅ Easier debugging with uncompressed files and browser DevTools
- ✅ No CORS issues
- ✅ Transparent API passthrough

### Watch Mode

For active development, use watch mode to automatically rebuild on file changes:

```bash
npm run watch
```

This will monitor:
- `web/game/src/**/*`
- `web/system/src/**/*`
- `web/game/public/**/*`
- `web/system/public/**/*`

**Combine with proxy server for best experience:**
```bash
# Terminal 1: Watch for changes and auto-rebuild
npm run watch

# Terminal 2: Run proxy server
npm run proxy -- 8080 /api 192.168.4.1
```

Now any changes to React components will automatically rebuild, and you just need to refresh your browser!

### Build Individual Apps

**Game interface only:**
```bash
grunt build-game
```

**System interface only:**
```bash
grunt build-system
```

### Gzip Compression

Run gzip compression separately (already included in `npm run build`):
```bash
npm run gzip
```

This will:
- Find all `.js`, `.css`, and `.html` files in `data/web/*`
- Compress each file using gzip
- Save as `.js.gz`, `.css.gz`, `.html.gz`
- Delete the original uncompressed files

**Note:** The ESP32 webserver should be configured to serve `.gz` files with proper Content-Encoding headers.

### Clean Build

Remove all build artifacts:
```bash
npm run clean
```

## Project Structure

```
web/
├── game/
│   ├── src/
│   │   ├── App.js          # Main game component
│   │   ├── index.js        # Entry point
│   │   └── styles.css      # Game styles
│   └── public/
│       └── index.html      # HTML template
└── system/
    ├── src/
    │   ├── App.js          # Main system component
    │   ├── index.js        # Entry point
    │   └── styles.css      # System styles
    └── public/
        └── index.html      # HTML template
```

## Build Output

After running `npm run build`, you'll find:

```
dist/
├── game/
│   ├── index.html
│   ├── bundle.js
│   └── images/
└── system/
    ├── index.html
    ├── bundle.js
    └── images/

data/web/
├── game/
│   ├── index.html
│   ├── bundle.js
│   └── images/
└── system/
    ├── index.html
    ├── bundle.js
    └── images/
```

## Accessing the UIs

After uploading to ESP32:

1. Connect to WiFi: `ESP32-WebServer` (password: `12345678`)
2. Open browser:
   - **Game UI:** `http://192.168.4.1/web/game/`
   - **System UI:** `http://192.168.4.1/web/system/`

## Troubleshooting

### Build fails with "Module not found"
```bash
rm -rf node_modules package-lock.json
npm install
```

### SPIFFS upload fails
- Check that `data/` directory exists
- Verify `platformio.ini` has `board_build.filesystem = spiffs`
- Ensure partition table has enough space for SPIFFS

### React app shows blank page
- Check browser console for errors
- Verify API endpoints are accessible
- Check that bundle.js is loaded correctly

## Adding New Features

### Add a new React component

1. Create component in `web/game/src/` or `web/system/src/`
2. Import in `App.js`
3. Run `npm run build`
4. Upload filesystem to ESP32

### Add images or assets

1. Place files in `web/game/public/images/` or `web/system/public/images/`
2. Reference in React: `<img src="images/myimage.png" />`
3. Run `npm run build` (Grunt will copy them automatically)

### Modify styles

1. Edit `web/game/src/styles.css` or `web/system/src/styles.css`
2. Run `npm run build`
3. Upload filesystem to ESP32

