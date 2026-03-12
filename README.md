# ESP32 Webserver with React UI

This project contains an ESP32-based webserver with two standalone React applications for Game and System interfaces.

## Project Structure

```
.
├── web/                    # React source code
│   ├── game/              # Game interface React app
│   │   ├── src/
│   │   │   ├── App.js
│   │   │   ├── index.js
│   │   │   └── styles.css
│   │   └── public/
│   │       └── index.html
│   └── system/            # System interface React app
│       ├── src/
│       │   ├── App.js
│       │   ├── index.js
│       │   └── styles.css
│       └── public/
│           └── index.html
├── data/                  # SPIFFS filesystem (deployed builds)
│   └── web/
│       ├── game/          # Compiled game UI
│       └── system/        # Compiled system UI
├── src/                   # ESP32 C++ source code
├── include/               # ESP32 headers
├── Gruntfile.js          # Build automation
├── webpack.config.js     # Webpack configuration
└── package.json          # Node.js dependencies

```

## Setup

### 1. Install Node.js Dependencies

```bash
npm install
```

### 2. Build React Applications

Build both applications and deploy to `data/web/`:

```bash
npm run build
```

Or use Grunt directly:

```bash
grunt build
```

This will:
- Compile React applications with Webpack
- Copy files to `data/web/game/` and `data/web/system/`
- **Gzip compress** all `.js`, `.css`, and `.html` files
- Delete original uncompressed files (only `.gz` versions remain)

### 3. Gzip Compression

Run gzip compression separately (if needed):
```bash
npm run gzip
```

This compresses all `.js`, `.css`, and `.html` files in `data/web/*` and replaces them with `.gz` versions.

### 4. Build Individual Apps

Build only the game interface:
```bash
grunt build-game
```

Build only the system interface:
```bash
grunt build-system
```

### 4. Watch for Changes

Automatically rebuild on file changes:

```bash
npm run watch
```

### 5. Upload to ESP32

After building, upload the filesystem:

```bash
pio run --target uploadfs
```

Then upload the firmware:

```bash
pio run --target upload
```

## React Applications

### Game Interface (`/web/game`)

- Interactive game controls
- Real-time game state display
- Score tracking
- API integration with `/api/game/*` endpoints

**Access:** `http://192.168.4.1/web/game/`

### System Interface (`/web/system`)

- Device information display
- System status monitoring
- WiFi information
- Device restart functionality
- API integration with `/api/system/*` endpoints

**Access:** `http://192.168.4.1/web/system/`

## API Endpoints

### Game API
- `GET /api/game/state` - Get game state
- `POST /api/game/state` - Set game state
- `GET /api/game/score` - Get score
- `POST /api/game/score` - Set score
- `POST /api/game/reset` - Reset game

### System API
- `GET /api/system/info` - Device information
- `GET /api/system/status` - System status
- `GET /api/system/wifi` - WiFi information
- `POST /api/system/restart` - Restart device

## Development Workflow

### Standard Workflow (Upload to ESP32)

1. Edit React components in `web/game/src/` or `web/system/src/`
2. Run `npm run build` to compile
3. Compiled files are automatically copied to `data/web/game/` and `data/web/system/`
4. Upload filesystem to ESP32 with `pio run --target uploadfs`

### Development Proxy Server (Faster Iteration)

For faster development without uploading to SPIFFS on every change, use the development proxy server:

```bash
npm run proxy -- <port> <apiPath> <esp32Host>
```

**Example:**
```bash
npm run proxy -- 8080 /api myESP32.local
```

**Parameters:**
- `port` - Local port for the proxy server (e.g., `8080`)
- `apiPath` - API root path to forward to ESP32 (e.g., `/api`)
- `esp32Host` - ESP32 hostname or IP address (e.g., `myESP32.local`, `192.168.4.1`)

**How it works:**
- **Static files** (non-API requests) are served from local `/dist` directory
  - `http://localhost:8080/game/` → serves `/dist/game/index.html`
  - `http://localhost:8080/system/` → serves `/dist/system/index.html`
- **API requests** (starting with `/api`) are transparently forwarded to the ESP32
  - `http://localhost:8080/api/game/state` → proxied to `http://myESP32.local/api/game/state`
  - All headers, cookies, methods, and response data are passed through unchanged

**Benefits:**
- No SPIFFS upload needed for UI changes
- Faster development iteration
- Test against real ESP32 APIs
- Easier debugging with uncompressed files

## Technologies Used

- **Frontend:** React 18, CSS3
- **Build Tools:** Webpack 5, Grunt, Babel
- **Backend:** ESP32 (Arduino framework)
- **Server:** WebServer library, SPIFFS filesystem
- **APIs:** RESTful JSON APIs

## License

MIT

