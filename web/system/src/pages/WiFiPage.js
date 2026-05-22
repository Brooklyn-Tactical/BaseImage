import React, { useState, useEffect } from 'react';
import {
  Box, Typography, Stack, TextField,
  FormControl, InputLabel, Select, MenuItem, Button,
} from '@mui/material';

const DEFAULT_FORM = { channel: 1, apName: '', apPassword: '' };

function normalize(data) {
  return {
    channel:    data.channel  ?? 1,
    apName:     data.ssid     ?? '',
    apPassword: data.password ?? '',
  };
}

export default function WiFiPage({ onSnackbar }) {
  const [saved, setSaved] = useState(DEFAULT_FORM);
  const [form, setForm] = useState(DEFAULT_FORM);

  useEffect(() => {
    fetch('/api/system/wifi')
      .then(r => r.json())
      .then(data => {
        const vals = normalize(data);
        setSaved(vals);
        setForm(vals);
      })
      .catch(() => onSnackbar('error', 'Failed to load WiFi settings'));
  }, []);

  const handleChange = (field) => (e) =>
    setForm(f => ({ ...f, [field]: e.target.value }));

  const handleSave = async () => {
    try {
      const r = await fetch('/api/system/wifi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          channel:  form.channel,
          ssid:     form.apName,
          password: form.apPassword,
        }),
      });
      if (r.ok) {
        setSaved(form);
        onSnackbar('success', 'WiFi settings saved');
      } else {
        const body = await r.json().catch(() => ({}));
        const detail = body.error ? `: ${body.error}` : '';
        onSnackbar('error', `Save failed [HTTP ${r.status}]${detail}`);
      }
    } catch (err) {
      onSnackbar('error', `Save failed: ${err.message}`);
    }
  };

  const handleCancel = () => setForm(saved);

  return (
    <Box sx={{ p: 3 }}>
      <Typography variant="h5" gutterBottom>WiFi Configuration</Typography>
      <Stack spacing={3} sx={{ maxWidth: 400 }}>
        <FormControl fullWidth>
          <InputLabel id="wifi-channel-label">WiFi Channel</InputLabel>
          <Select
            labelId="wifi-channel-label"
            value={form.channel}
            label="WiFi Channel"
            onChange={handleChange('channel')}
          >
            {Array.from({ length: 11 }, (_, i) => i + 1).map(ch => (
              <MenuItem key={ch} value={ch}>{ch}</MenuItem>
            ))}
          </Select>
        </FormControl>

        <TextField
          label="AP Name"
          value={form.apName}
          onChange={handleChange('apName')}
          inputProps={{ maxLength: 31 }}
          helperText={`${form.apName.length}/31`}
          fullWidth
        />

        <TextField
          label="AP Password"
          value={form.apPassword}
          onChange={handleChange('apPassword')}
          fullWidth
        />

        <Stack direction="row" spacing={2}>
          <Button variant="contained" onClick={handleSave}>Save</Button>
          <Button variant="outlined" onClick={handleCancel}>Cancel</Button>
        </Stack>
      </Stack>
    </Box>
  );
}

