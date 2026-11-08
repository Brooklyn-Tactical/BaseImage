import React, { useState, useEffect } from 'react';
import {
  Box, Typography, Stack, TextField,
  FormControl, InputLabel, Select, MenuItem, Button,
} from '@mui/material';
import { useTranslation } from 'react-i18next';

const DEFAULT_FORM = { channel: 1, apName: '', apPassword: '' };

function normalize(data) {
  return {
    channel:    data.channel  ?? 1,
    apName:     data.ssid     ?? '',
    apPassword: data.password ?? '',
  };
}

export default function WiFiPage({ onSnackbar }) {
  const { t } = useTranslation(['system', 'common']);
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
      .catch(() => onSnackbar('error', t('system:wifi.snackbar.loadFailed')));
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
        onSnackbar('success', t('system:wifi.snackbar.saved'));
      } else {
        const body = await r.json().catch(() => ({}));
        const detail = body.error ? `: ${body.error}` : '';
        onSnackbar('error', t('common:error.httpSaveFailed', { status: r.status, detail }));
      }
    } catch (err) {
      onSnackbar('error', t('common:error.netSaveFailed', { message: err.message }));
    }
  };

  const handleCancel = () => setForm(saved);

  return (
    <Box sx={{ p: 3 }}>
      <Typography variant="h5" gutterBottom>{t('system:wifi.title')}</Typography>
      <Stack spacing={3} sx={{ maxWidth: 400 }}>
        <FormControl fullWidth>
          <InputLabel id="wifi-channel-label">{t('system:wifi.channel')}</InputLabel>
          <Select
            labelId="wifi-channel-label"
            value={form.channel}
            label={t('system:wifi.channel')}
            onChange={handleChange('channel')}
          >
            {Array.from({ length: 11 }, (_, i) => i + 1).map(ch => (
              <MenuItem key={ch} value={ch}>{ch}</MenuItem>
            ))}
          </Select>
        </FormControl>

        <TextField
          label={t('system:wifi.apName')}
          value={form.apName}
          onChange={handleChange('apName')}
          inputProps={{ maxLength: 31 }}
          helperText={t('system:wifi.apNameCharCount', { len: form.apName.length })}
          fullWidth
        />

        <TextField
          label={t('system:wifi.apPassword')}
          value={form.apPassword}
          onChange={handleChange('apPassword')}
          fullWidth
        />

        <Stack direction="row" spacing={2}>
          <Button variant="contained" onClick={handleSave}>{t('common:action.save')}</Button>
          <Button variant="outlined" onClick={handleCancel}>{t('common:action.cancel')}</Button>
        </Stack>
      </Stack>
    </Box>
  );
}

