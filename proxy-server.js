const express = require('express');
const http = require('http');
const path = require('path');

// Parse command-line arguments
const args = process.argv.slice(2);

if (args.length !== 3) {
  console.error('Usage: npm run proxy -- <port> <apiPath> <esp32Host>');
  console.error('Example: npm run proxy -- 8080 /api myESP32.local');
  process.exit(1);
}

const [port, apiPath, esp32Host] = args;

// Validate port number
const portNum = parseInt(port, 10);
if (isNaN(portNum) || portNum < 1 || portNum > 65535) {
  console.error(`Error: Invalid port number "${port}". Must be between 1 and 65535.`);
  process.exit(1);
}

// Validate API path
if (!apiPath.startsWith('/')) {
  console.error(`Error: API path "${apiPath}" must start with "/"`);
  process.exit(1);
}

// Parse ESP32 host and port
let esp32Hostname;
let esp32Port = 80; // Default port

if (esp32Host.includes(':')) {
  const parts = esp32Host.split(':');
  esp32Hostname = parts[0];
  esp32Port = parseInt(parts[1], 10);

  if (isNaN(esp32Port) || esp32Port < 1 || esp32Port > 65535) {
    console.error(`Error: Invalid ESP32 port number "${parts[1]}". Must be between 1 and 65535.`);
    process.exit(1);
  }
} else {
  esp32Hostname = esp32Host;
}

// Create Express app
const app = express();

// Middleware to parse raw request body for proxying
app.use(apiPath, express.raw({ type: '*/*', limit: '10mb' }));

// Custom proxy handler for API requests
app.use(apiPath, (req, res) => {
  let fullPath = `${apiPath}${req.url}`;
  console.log(`[PROXY] ${req.method} ${req.url} -> http://${esp32Hostname}:${esp32Port}${fullPath}`);

  // Prepare options for HTTP client request to ESP32
  const options = {
    hostname: esp32Hostname,
    port: esp32Port,
    path: fullPath,
    method: req.method,
    headers: { ...req.headers }
  };

  // Remove host header to avoid conflicts
  delete options.headers.host;

  // Create HTTP client request to ESP32
  const proxyReq = http.request(options, (proxyRes) => {
    console.log(`[PROXY] ${req.method} ${fullPath} <- ${proxyRes.statusCode}`);

    // Copy status code from ESP32 response
    res.statusCode = proxyRes.statusCode;

    // Copy all headers from ESP32 response
    Object.keys(proxyRes.headers).forEach(key => {
      res.setHeader(key, proxyRes.headers[key]);
    });

    // Pipe the response body from ESP32 to client
    proxyRes.pipe(res);

    proxyRes.on('end', ()=> {

    });
  });

  // Handle errors
  proxyReq.on('error', (err) => {
    console.error(`[PROXY ERROR] ${req.method} ${fullPath}:`, err.message);
    if (!res.headersSent) {
      res.status(502).send('Bad Gateway - Could not connect to ESP32');
    }
  });

  // Forward request body if present
  if (req.body && req.body.length > 0) {
    proxyReq.write(req.body);
  }

  // End the request
  proxyReq.end();
});

// Serve shared assets (fonts, images, locale JSON files) from web/shared/
// This mirrors the ESP32 route: server.serveStatic("/shared/", FILESYSTEM, "/web/shared/")
const sharedPath = path.join(__dirname, 'web', 'shared');
app.use('/shared', express.static(sharedPath));

// Serve static files from /dist directory
const distPath = path.join(__dirname, 'dist');
app.use(express.static(distPath, {
  index: 'index.html',
  extensions: ['html']
}));

// Fallback for SPA routing - serve index.html for any unmatched routes
app.get(/.*/, (req, res) => {
  console.log(`[STATIC] ${req.method} ${req.url} -> ${distPath}${req.url}`);
  res.sendFile(path.join(distPath, req.url), (err) => {
    if (err) {
      console.error(`[STATIC ERROR] Could not serve ${req.url}:`, err.message);
      res.status(404).send('File not found');
    }
  });
});

// Start the server
app.listen(portNum, () => {
  console.log('='.repeat(60));
  console.log('ESP32 Development Proxy Server');
  console.log('='.repeat(60));
  console.log(`Local server:  http://localhost:${portNum}`);
  console.log(`API proxy:     ${apiPath}/* -> http://${esp32Hostname}:${esp32Port}${apiPath}/*`);
  console.log(`Static files:  /* -> ${distPath}/*`);
  console.log('='.repeat(60));
  console.log('Press Ctrl+C to stop the server');
  console.log('');
});

