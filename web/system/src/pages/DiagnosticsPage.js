import React, { useState, useEffect } from 'react';
import {
  Box, Typography, Stack, Switch, Button, TextField,
  FormControlLabel, Divider,
} from '@mui/material';

const HEX_RE = /^[0-9A-Fa-f]*$/;

export default function DiagnosticsPage({ onSnackbar }) {
  const [ledOn, setLedOn] = useState(false);
  const [displayOn, setDisplayOn] = useState(false);
  const [armStatus, setArmStatus] = useState('—');
  const [eepromText, setEepromText] = useState('');
  const [eepromSize, setEepromSize] = useState(0);
  const [rebooting, setRebooting] = useState(false);

  // Discover EEPROM size on mount so write validation works before first Read.
  useEffect(() => {
    fetch('/api/system/diag/eeprom')
      .then(r => r.json())
      .then(data => setEepromSize(data.size ?? 0))
      .catch(() => { /* size stays 0; surfaces as a validation error on Write */ });
  }, []);

  const handleLedToggle = async (e) => {
    const next = e.target.checked;
    setLedOn(next);
    try {
      const r = await fetch('/api/system/diag/led', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ on: next }),
      });
      if (!r.ok) {
        setLedOn(!next);
        onSnackbar('error', `LED set failed [HTTP ${r.status}]`);
      }
    } catch (err) {
      setLedOn(!next);
      onSnackbar('error', `LED set failed: ${err.message}`);
    }
  };

  const handleDisplayToggle = async (e) => {
    const next = e.target.checked;
    setDisplayOn(next);
    try {
      const r = await fetch('/api/system/diag/display', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ on: next }),
      });
      if (!r.ok) {
        setDisplayOn(!next);
        onSnackbar('error', `Display set failed [HTTP ${r.status}]`);
      }
    } catch (err) {
      setDisplayOn(!next);
      onSnackbar('error', `Display set failed: ${err.message}`);
    }
  };

  const handleBeep = async () => {
    try {
      const r = await fetch('/api/system/diag/beep', { method: 'POST' });
      if (!r.ok) onSnackbar('error', `Beep failed [HTTP ${r.status}]`);
    } catch (err) {
      onSnackbar('error', `Beep failed: ${err.message}`);
    }
  };

  const handleReadStatus = async () => {
    try {
      const r = await fetch('/api/system/diag/armstatus');
      if (!r.ok) {
        setArmStatus('error');
        onSnackbar('error', `Read Status failed [HTTP ${r.status}]`);
        return;
      }
      const data = await r.json();
      setArmStatus(data.status ?? 'unknown');
    } catch (err) {
      setArmStatus('error');
      onSnackbar('error', `Read Status failed: ${err.message}`);
    }
  };

  const handleEepromRead = async () => {
    try {
      const r = await fetch('/api/system/diag/eeprom');
      if (!r.ok) {
        onSnackbar('error', `EEPROM read failed [HTTP ${r.status}]`);
        return;
      }
      const data = await r.json();
      setEepromText(data.hex ?? '');
      if (data.size != null) setEepromSize(data.size);
    } catch (err) {
      onSnackbar('error', `EEPROM read failed: ${err.message}`);
    }
  };

  const expectedHexLen = eepromSize * 2;
  const eepromHexValid = HEX_RE.test(eepromText);
  const eepromLenValid = expectedHexLen > 0 && eepromText.length === expectedHexLen;
  const eepromError = eepromText.length > 0 && (!eepromHexValid || !eepromLenValid);
  const eepromHelper = !eepromHexValid
    ? 'Only 0–9 and A–F allowed'
    : (expectedHexLen > 0
        ? `${eepromText.length}/${expectedHexLen} hex characters (${eepromSize} bytes)`
        : `${eepromText.length} hex characters`);

  const handleReboot = async () => {
    setRebooting(true);
    try {
      await fetch('/api/system/restart', { method: 'POST' });
      onSnackbar('info', 'Rebooting device — reconnect in a few seconds');
    } catch (err) {
      // The device restarts before it can finish the TCP close, so a network
      // error here is expected and does not mean the reboot failed.
      onSnackbar('info', 'Rebooting device — reconnect in a few seconds');
    }
  };

  const handleEepromWrite = async () => {
    if (!eepromHexValid || !eepromLenValid) {
      onSnackbar('error', `EEPROM contents invalid: expected ${expectedHexLen} hex characters`);
      return;
    }
    try {
      const r = await fetch('/api/system/diag/eeprom', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ hex: eepromText.toUpperCase() }),
      });
      if (r.ok) onSnackbar('success', 'EEPROM written');
      else      onSnackbar('error', `EEPROM write failed [HTTP ${r.status}]`);
    } catch (err) {
      onSnackbar('error', `EEPROM write failed: ${err.message}`);
    }
  };

  return (
    <Box sx={{ p: 3 }}>
      <Typography variant="h5" gutterBottom>Diagnostics</Typography>
      <Stack spacing={3} sx={{ maxWidth: 520 }} divider={<Divider flexItem />}>

        <Box>
          <Typography variant="subtitle1" gutterBottom>LED Bar</Typography>
          <FormControlLabel
            control={<Switch checked={ledOn} onChange={handleLedToggle} />}
            label={ledOn ? 'On' : 'Off'}
          />
        </Box>

        <Box>
          <Typography variant="subtitle1" gutterBottom>LED Display</Typography>
          <FormControlLabel
            control={<Switch checked={displayOn} onChange={handleDisplayToggle} />}
            label={displayOn ? 'On' : 'Off'}
          />
        </Box>

        <Box>
          <Typography variant="subtitle1" gutterBottom>Speaker</Typography>
          <Button variant="contained" onClick={handleBeep}>Beep</Button>
        </Box>

        <Box>
          <Typography variant="subtitle1" gutterBottom>Arm/Disarm</Typography>
          <Stack direction="row" spacing={2} alignItems="center">
            <Button variant="contained" onClick={handleReadStatus}>Read Status</Button>
            <Typography variant="body1">Status: {armStatus}</Typography>
          </Stack>
        </Box>

        <Box>
          <Typography variant="subtitle1" gutterBottom>EEPROM</Typography>
          <TextField
            value={eepromText}
            onChange={(e) => setEepromText(e.target.value)}
            error={eepromError}
            helperText={eepromHelper}
            multiline
            minRows={3}
            fullWidth
            inputProps={{ style: { fontFamily: 'monospace' } }}
          />
          <Stack direction="row" spacing={2} sx={{ mt: 1 }}>
            <Button variant="outlined"  onClick={handleEepromRead}>Read</Button>
            <Button variant="contained" onClick={handleEepromWrite}
              disabled={!eepromHexValid || !eepromLenValid}>Write</Button>
          </Stack>
        </Box>

        <Box>
          <Typography variant="subtitle1" gutterBottom>System</Typography>
          <Button
            variant="contained"
            color="error"
            disabled={rebooting}
            onClick={handleReboot}
          >
            {rebooting ? 'Rebooting…' : 'Reboot'}
          </Button>
        </Box>

      </Stack>
    </Box>
  );
}

