import React, { useState, useEffect } from 'react';
import {
  Box, Typography, Stack, Switch, Button, TextField,
  FormControlLabel, Divider,
} from '@mui/material';
import { useTranslation } from 'react-i18next';

const HEX_RE = /^[0-9A-Fa-f]*$/;

export default function DiagnosticsPage({ onSnackbar }) {
  const { t } = useTranslation(['system', 'common']);
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
        onSnackbar('error', t('system:diagnostics.snackbar.ledFailed', { status: r.status }));
      }
    } catch (err) {
      setLedOn(!next);
      onSnackbar('error', t('system:diagnostics.snackbar.ledFailedNet', { message: err.message }));
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
        onSnackbar('error', t('system:diagnostics.snackbar.displayFailed', { status: r.status }));
      }
    } catch (err) {
      setDisplayOn(!next);
      onSnackbar('error', t('system:diagnostics.snackbar.displayFailedNet', { message: err.message }));
    }
  };

  const handleBeep = async () => {
    try {
      const r = await fetch('/api/system/diag/beep', { method: 'POST' });
      if (!r.ok) onSnackbar('error', t('system:diagnostics.snackbar.beepFailed', { status: r.status }));
    } catch (err) {
      onSnackbar('error', t('system:diagnostics.snackbar.beepFailedNet', { message: err.message }));
    }
  };

  const handleDemo = async (action) => {
    try {
      const r = await fetch(`/api/system/diag/demo/${action}`, { method: 'POST' });
      if (!r.ok) onSnackbar('error', t('system:diagnostics.snackbar.demoFailed', { status: r.status }));
    } catch (err) {
      onSnackbar('error', t('system:diagnostics.snackbar.demoFailedNet', { message: err.message }));
    }
  };

  const handleReadStatus = async () => {
    try {
      const r = await fetch('/api/system/diag/armstatus');
      if (!r.ok) {
        setArmStatus('error');
        onSnackbar('error', t('system:diagnostics.snackbar.readStatusFailed', { status: r.status }));
        return;
      }
      const data = await r.json();
      setArmStatus(data.status ?? 'unknown');
    } catch (err) {
      setArmStatus('error');
      onSnackbar('error', t('system:diagnostics.snackbar.readStatusFailedNet', { message: err.message }));
    }
  };

  const handleEepromRead = async () => {
    try {
      const r = await fetch('/api/system/diag/eeprom');
      if (!r.ok) {
        onSnackbar('error', t('system:diagnostics.snackbar.eepromReadFailed', { status: r.status }));
        return;
      }
      const data = await r.json();
      setEepromText(data.hex ?? '');
      if (data.size != null) setEepromSize(data.size);
    } catch (err) {
      onSnackbar('error', t('system:diagnostics.snackbar.eepromReadFailedNet', { message: err.message }));
    }
  };

  const expectedHexLen = eepromSize * 2;
  const eepromHexValid = HEX_RE.test(eepromText);
  const eepromLenValid = expectedHexLen > 0 && eepromText.length === expectedHexLen;
  const eepromError = eepromText.length > 0 && (!eepromHexValid || !eepromLenValid);
  const eepromHelper = !eepromHexValid
    ? t('system:diagnostics.eeprom.invalidChars')
    : (expectedHexLen > 0
        ? t('system:diagnostics.eeprom.charCountFull', { len: eepromText.length, expected: expectedHexLen, bytes: eepromSize })
        : t('system:diagnostics.eeprom.charCount', { len: eepromText.length }));

  const handleReboot = async () => {
    setRebooting(true);
    try {
      await fetch('/api/system/restart', { method: 'POST' });
      onSnackbar('info', t('system:diagnostics.snackbar.rebooting'));
    } catch (err) {
      // The device restarts before it can finish the TCP close, so a network
      // error here is expected and does not mean the reboot failed.
      onSnackbar('info', t('system:diagnostics.snackbar.rebooting'));
    }
  };

  const handleEepromWrite = async () => {
    if (!eepromHexValid || !eepromLenValid) {
      onSnackbar('error', t('system:diagnostics.snackbar.eepromInvalid', { expected: expectedHexLen }));
      return;
    }
    try {
      const r = await fetch('/api/system/diag/eeprom', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ hex: eepromText.toUpperCase() }),
      });
      if (r.ok) onSnackbar('success', t('system:diagnostics.snackbar.eepromWritten'));
      else      onSnackbar('error', t('system:diagnostics.snackbar.eepromWriteFailed', { status: r.status }));
    } catch (err) {
      onSnackbar('error', t('system:diagnostics.snackbar.eepromWriteFailedNet', { message: err.message }));
    }
  };

  return (
    <Box sx={{ p: 3 }}>
      <Typography variant="h5" gutterBottom>{t('system:diagnostics.title')}</Typography>
      <Stack spacing={3} sx={{ maxWidth: 520 }} divider={<Divider flexItem />}>

        <Box>
          <Typography variant="subtitle1" gutterBottom>{t('system:diagnostics.ledBar.title')}</Typography>
          <FormControlLabel
            control={<Switch checked={ledOn} onChange={handleLedToggle} />}
            label={ledOn ? t('common:onOff.on') : t('common:onOff.off')}
          />
        </Box>

        <Box>
          <Typography variant="subtitle1" gutterBottom>{t('system:diagnostics.ledDisplay.title')}</Typography>
          <FormControlLabel
            control={<Switch checked={displayOn} onChange={handleDisplayToggle} />}
            label={displayOn ? t('common:onOff.on') : t('common:onOff.off')}
          />
        </Box>

        <Box>
          <Typography variant="subtitle1" gutterBottom>{t('system:diagnostics.speaker.title')}</Typography>
          <Button variant="contained" onClick={handleBeep}>{t('system:diagnostics.speaker.beep')}</Button>
        </Box>

        <Box>
          <Typography variant="subtitle1" gutterBottom>{t('system:diagnostics.demo.title')}</Typography>
          <Stack direction="row" spacing={2}>
            <Button variant="contained" onClick={() => handleDemo('start')}>{t('system:diagnostics.demo.start')}</Button>
            <Button variant="outlined" color="error" onClick={() => handleDemo('stop')}>{t('system:diagnostics.demo.stop')}</Button>
          </Stack>
        </Box>

        <Box>
          <Typography variant="subtitle1" gutterBottom>{t('system:diagnostics.armDisarm.title')}</Typography>
          <Stack direction="row" spacing={2} alignItems="center">
            <Button variant="contained" onClick={handleReadStatus}>{t('system:diagnostics.armDisarm.readStatus')}</Button>
            <Typography variant="body1">{t('system:diagnostics.armDisarm.status', { value: armStatus })}</Typography>
          </Stack>
        </Box>

        <Box>
          <Typography variant="subtitle1" gutterBottom>{t('system:diagnostics.eeprom.title')}</Typography>
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
            <Button variant="outlined"  onClick={handleEepromRead}>{t('common:action.read')}</Button>
            <Button variant="contained" onClick={handleEepromWrite}
              disabled={!eepromHexValid || !eepromLenValid}>{t('common:action.write')}</Button>
          </Stack>
        </Box>

        <Box>
          <Typography variant="subtitle1" gutterBottom>{t('system:diagnostics.system.title')}</Typography>
          <Button
            variant="contained"
            color="error"
            disabled={rebooting}
            onClick={handleReboot}
          >
            {rebooting ? t('system:diagnostics.system.rebooting') : t('system:diagnostics.system.reboot')}
          </Button>
        </Box>

      </Stack>
    </Box>
  );
}

