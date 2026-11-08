import React, { useState, useEffect } from 'react';
import { Box, Typography, Stack, TextField, Button } from '@mui/material';
import { useTranslation } from 'react-i18next';

const DEFAULT_FORM = { gameNet: '', gameMode: '', meshNode: '', meshName: '' };

function isUint8(v) {
  const n = parseInt(v, 10);
  return !isNaN(n) && n >= 0 && n <= 255 && String(n) === v.trim();
}

function isUint16(v) {
  const n = parseInt(v, 10);
  return !isNaN(n) && n >= 0 && n <= 65535 && String(n) === v.trim();
}

function normalize(data) {
  return {
    gameNet:  data.net_id      != null ? String(data.net_id)      : '',
    gameMode: data.mode_id     != null ? String(data.mode_id)     : '',
    meshNode: data.device_id   != null ? String(data.device_id)   : '',
    meshName: data.description ?? '',
  };
}

export default function MeshPage({ onSnackbar }) {
  const { t } = useTranslation(['system', 'common']);
  const [saved, setSaved] = useState(DEFAULT_FORM);
  const [form, setForm] = useState(DEFAULT_FORM);

  useEffect(() => {
    fetch('/api/system/mesh')
      .then(r => r.json())
      .then(data => {
        const vals = normalize(data);
        setSaved(vals);
        setForm(vals);
      })
      .catch(() => onSnackbar('error', t('system:mesh.snackbar.loadFailed')));
  }, []);

  const handleChange = (field) => (e) => {
    const val = field === 'meshName' ? e.target.value.slice(0, 24) : e.target.value;
    setForm(f => ({ ...f, [field]: val }));
  };

  const errors = {
    gameNet: form.gameNet !== '' && !isUint8(form.gameNet),
    gameMode: form.gameMode !== '' && !isUint8(form.gameMode),
    meshNode: form.meshNode !== '' && !isUint16(form.meshNode),
    meshName: false,
  };

  const hasErrors = Object.values(errors).some(Boolean);

  const handleSave = async () => {
    try {
      const r = await fetch('/api/system/mesh', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          net_id:      parseInt(form.gameNet,  10),
          mode_id:     parseInt(form.gameMode, 10),
          device_id:   parseInt(form.meshNode, 10),
          description: form.meshName,
        }),
      });
      if (r.ok) {
        setSaved(form);
        onSnackbar('success', t('system:mesh.snackbar.saved'));
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
      <Typography variant="h5" gutterBottom>{t('system:mesh.title')}</Typography>
      <Stack spacing={3} sx={{ maxWidth: 400 }}>
        <TextField
          label={t('system:mesh.gameNet')}
          value={form.gameNet}
          onChange={handleChange('gameNet')}
          error={errors.gameNet}
          helperText={errors.gameNet ? t('system:mesh.error.uint8') : ' '}
          inputProps={{ inputMode: 'numeric' }}
          fullWidth
        />
        <TextField
          label={t('system:mesh.gameMode')}
          value={form.gameMode}
          onChange={handleChange('gameMode')}
          error={errors.gameMode}
          helperText={errors.gameMode ? t('system:mesh.error.uint8') : ' '}
          inputProps={{ inputMode: 'numeric' }}
          fullWidth
        />
        <TextField
          label={t('system:mesh.meshNode')}
          value={form.meshNode}
          onChange={handleChange('meshNode')}
          error={errors.meshNode}
          helperText={errors.meshNode ? t('system:mesh.error.uint16') : ' '}
          inputProps={{ inputMode: 'numeric' }}
          fullWidth
        />
        <TextField
          label={t('system:mesh.meshName')}
          value={form.meshName}
          onChange={handleChange('meshName')}
          helperText={t('system:mesh.nameCharCount', { len: form.meshName.length })}
          inputProps={{ maxLength: 28 }}
          fullWidth
        />
        <Stack direction="row" spacing={2}>
          <Button variant="contained" onClick={handleSave} disabled={hasErrors}>{t('common:action.save')}</Button>
          <Button variant="outlined" onClick={handleCancel}>{t('common:action.cancel')}</Button>
        </Stack>
      </Stack>
    </Box>
  );
}

