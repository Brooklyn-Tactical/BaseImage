import React, { useState, useRef } from 'react';
import {
  Box, Typography, Stack, Button,
  Dialog, DialogTitle, DialogContent, DialogContentText, DialogActions,
  Paper,
} from '@mui/material';

const FIRMWARE_WARNING =
  'You are about to upload new firmware to your device. This process should only ' +
  'be performed with a full battery charge. Once started, do not interrupt the ' +
  'update — doing so may corrupt the device firmware and require manual recovery.';

const FILESYSTEM_WARNING =
  'You are about to update your device\'s file system contents. This process should ' +
  'only be performed with a full battery charge. Once started, do not interrupt the ' +
  'update — doing so may corrupt the filesystem and require manual recovery.';

const CAPTIVE_NOTE =
  'Many devices block file uploads from the built-in captive-portal browser. ' +
  'If you cannot select a file, connect to this network first and then open ' +
  'http://my.milsim.prop/ in your normal browser.';

// ── localStorage key prefix for lifted files ─────────────────────────────
const LS_PREFIX = 'lifted:';

// ── Filesystem OTA pipeline ───────────────────────────────────────────────
async function runFilesystemOta(file, log) {
  // 1. Fetch list of files to lift
  log('Fetching list of files to preserve…');
  const manifestRes = await fetch('/api/system/files');
  if (!manifestRes.ok) throw new Error('Could not read filesystem manifest');
  const { lifted_files } = await manifestRes.json();
  log(`Files to preserve: ${lifted_files.length > 0 ? lifted_files.join(', ') : 'none'}`);

  // 2. Download each file into localStorage
  log('Saving files to browser storage…');
  for (const name of lifted_files) {
    const path = name.startsWith('/') ? name : '/' + name;
    const res = await fetch(`/api/system/file?path=${encodeURIComponent(path)}`);
    if (res.ok) {
      localStorage.setItem(LS_PREFIX + path, await res.text());
      log(`  ✓ Saved ${path}`);
    } else {
      log(`  – ${path} not found on device, skipping`);
    }
  }

  // 3. Upload the filesystem image
  log('Uploading filesystem image to device…');
  const formData = new FormData();
  formData.append('file', file, file.name);
  const uploadRes = await fetch('/api/system/ota/filesystem', {
    method: 'POST',
    body: formData,
  });
  if (!uploadRes.ok) {
    const body = await uploadRes.json().catch(() => ({}));
    throw new Error(body.error || `Upload failed (HTTP ${uploadRes.status})`);
  }
  log('Filesystem flashed successfully.');

  // 4. Restore lifted files
  return restoreLifted(log);
}

// ── Restore files already in localStorage (also used as stand-alone retry) ─
async function restoreLifted(log) {
  const keys = Object.keys(localStorage).filter(k => k.startsWith(LS_PREFIX));
  if (keys.length === 0) { log('No files to restore.'); return true; }

  log(`Restoring ${keys.length} file(s) to device…`);
  let allOk = true;
  for (const key of keys) {
    const path = key.slice(LS_PREFIX.length);
    const content = localStorage.getItem(key);
    try {
      const res = await fetch(`/api/system/file?path=${encodeURIComponent(path)}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: content,
      });
      if (res.ok) {
        localStorage.removeItem(key);
        log(`  ✓ Restored ${path}`);
      } else {
        log(`  ✗ Failed to restore ${path} — kept in browser storage`);
        allOk = false;
      }
    } catch {
      log(`  ✗ Network error restoring ${path} — kept in browser storage`);
      allOk = false;
    }
  }
  return allOk;
}

// ─────────────────────────────────────────────────────────────────────────────

export default function OtaPage({ type }) {
  const isFirmware = type === 'firmware';
  const [selectedFile, setSelectedFile] = useState(null);
  const [dialogOpen, setDialogOpen]     = useState(false);

  // Filesystem-OTA state
  const [phase, setPhase]             = useState('idle'); // idle|running|done|error
  const [statusLines, setStatusLines] = useState([]);
  const logBoxRef = useRef(null);

  const hasPendingRestore =
    Object.keys(localStorage).some(k => k.startsWith(LS_PREFIX));

  const appendLog = (line) => {
    setStatusLines(prev => [...prev, line]);
    setTimeout(() => {
      if (logBoxRef.current)
        logBoxRef.current.scrollTop = logBoxRef.current.scrollHeight;
    }, 0);
  };

  const handleFileChange = (e) => setSelectedFile(e.target.files[0] || null);

  // ── Firmware: unchanged behaviour ────────────────────────────────────────
  if (isFirmware) {
    return (
      <Box sx={{ p: 3 }}>
        <Typography variant="h5" gutterBottom>Firmware Update</Typography>
        <Typography variant="body1" color="warning.main"
          sx={{ mb: 2, maxWidth: 520, fontWeight: 500 }}>
          {FIRMWARE_WARNING}
        </Typography>
        <Typography variant="body1" sx={{ mb: 3, maxWidth: 520 }}>{CAPTIVE_NOTE}</Typography>
        <Stack spacing={2} sx={{ maxWidth: 400 }}>
          <Button variant="outlined" component="label" fullWidth>
            {selectedFile ? selectedFile.name : 'Select File…'}
            <input type="file" hidden onChange={handleFileChange} />
          </Button>
          <Button variant="contained" color="error" disabled={!selectedFile}
            onClick={() => setDialogOpen(true)} fullWidth>
            Upload and Restart
          </Button>
        </Stack>
        <Dialog open={dialogOpen} onClose={() => setDialogOpen(false)}>
          <DialogTitle>Firmware OTA — Not Yet Implemented</DialogTitle>
          <DialogContent>
            <DialogContentText>
              The firmware OTA upload endpoint has not yet been configured on the device.
            </DialogContentText>
          </DialogContent>
          <DialogActions>
            <Button onClick={() => setDialogOpen(false)}>Close</Button>
          </DialogActions>
        </Dialog>
      </Box>
    );
  }

  // ── Filesystem pipeline ───────────────────────────────────────────────────
  const startPipeline = async () => {
    setDialogOpen(false);
    setPhase('running');
    setStatusLines([]);
    try {
      const allOk = await runFilesystemOta(selectedFile, appendLog);
      setPhase(allOk ? 'done' : 'error');
      if (allOk) appendLog('✓ Filesystem update complete!');
      else        appendLog('⚠ Some files could not be restored — see log above.');
    } catch (err) {
      appendLog(`✗ Error: ${err.message}`);
      setPhase('error');
    }
  };

  const startRetryRestore = async () => {
    setPhase('running');
    setStatusLines([]);
    try {
      const allOk = await restoreLifted(appendLog);
      setPhase(allOk ? 'done' : 'error');
      if (allOk) appendLog('✓ Restore complete!');
      else        appendLog('⚠ Some files could not be restored.');
    } catch (err) {
      appendLog(`✗ Error: ${err.message}`);
      setPhase('error');
    }
  };

  const isRunning = phase === 'running';

  return (
    <Box sx={{ p: 3 }}>
      <Typography variant="h5" gutterBottom>Filesystem Update</Typography>
      <Typography variant="body1" color="warning.main"
        sx={{ mb: 2, maxWidth: 520, fontWeight: 500 }}>
        {FILESYSTEM_WARNING}
      </Typography>
      <Typography variant="body1" sx={{ mb: 3, maxWidth: 520 }}>{CAPTIVE_NOTE}</Typography>

      <Stack spacing={2} sx={{ maxWidth: 400 }}>
        <Button variant="outlined" component="label" fullWidth disabled={isRunning}>
          {selectedFile ? selectedFile.name : 'Select File…'}
          <input type="file" hidden onChange={handleFileChange} />
        </Button>

        <Button variant="contained" color="error" fullWidth
          disabled={!selectedFile || isRunning}
          onClick={() => setDialogOpen(true)}>
          Upload and Apply
        </Button>

        {hasPendingRestore && phase === 'idle' && (
          <Button variant="outlined" color="warning" fullWidth
            onClick={startRetryRestore}>
            Retry Restore Saved Files
          </Button>
        )}
      </Stack>

      {statusLines.length > 0 && (
        <Paper variant="outlined" ref={logBoxRef}
          sx={{ mt: 3, p: 1.5, maxWidth: 520, maxHeight: 220, overflowY: 'auto',
                fontFamily: 'monospace', fontSize: '0.78rem', whiteSpace: 'pre-wrap' }}>
          {statusLines.map((l, i) => <div key={i}>{l}</div>)}
        </Paper>
      )}

      {phase === 'done' && (
        <Typography color="success.main" sx={{ mt: 2, fontWeight: 600 }}>
          Filesystem update applied successfully.
        </Typography>
      )}
      {phase === 'error' && (
        <Typography color="error.main" sx={{ mt: 2, fontWeight: 600 }}>
          Update finished with errors — check the log above.
        </Typography>
      )}

      <Dialog open={dialogOpen} onClose={() => !isRunning && setDialogOpen(false)}>
        <DialogTitle>Confirm Filesystem Update</DialogTitle>
        <DialogContent>
          <DialogContentText>
            This will save your current device settings to browser storage, flash the
            new filesystem image, then restore those settings automatically.
            Do not close this browser tab until the process is complete.
          </DialogContentText>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setDialogOpen(false)}>Cancel</Button>
          <Button variant="contained" color="error" onClick={startPipeline}>
            Proceed
          </Button>
        </DialogActions>
      </Dialog>
    </Box>
  );
}

