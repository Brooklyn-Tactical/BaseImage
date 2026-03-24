import React, { useState, useEffect } from 'react';
import { Box, Typography, Stack, TextField, Button } from '@mui/material';

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
      .catch(() => onSnackbar('Failed to load Mesh settings'));
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
        onSnackbar('Mesh settings saved');
      } else {
        const body = await r.json().catch(() => ({}));
        const detail = body.error ? `: ${body.error}` : '';
        onSnackbar(`Save failed [HTTP ${r.status}]${detail}`);
      }
    } catch (err) {
      onSnackbar(`Save failed: ${err.message}`);
    }
  };

  const handleCancel = () => setForm(saved);

  return (
    <Box sx={{ p: 3 }}>
      <Typography variant="h5" gutterBottom>Mesh Configuration</Typography>
      <Stack spacing={3} sx={{ maxWidth: 400 }}>
        <TextField
          label="Game Net #"
          value={form.gameNet}
          onChange={handleChange('gameNet')}
          error={errors.gameNet}
          helperText={errors.gameNet ? 'Must be 0–255' : ' '}
          inputProps={{ inputMode: 'numeric' }}
          fullWidth
        />
        <TextField
          label="Game Mode #"
          value={form.gameMode}
          onChange={handleChange('gameMode')}
          error={errors.gameMode}
          helperText={errors.gameMode ? 'Must be 0–255' : ' '}
          inputProps={{ inputMode: 'numeric' }}
          fullWidth
        />
        <TextField
          label="Mesh Node #"
          value={form.meshNode}
          onChange={handleChange('meshNode')}
          error={errors.meshNode}
          helperText={errors.meshNode ? 'Must be 0–65535' : ' '}
          inputProps={{ inputMode: 'numeric' }}
          fullWidth
        />
        <TextField
          label="Mesh Node Name"
          value={form.meshName}
          onChange={handleChange('meshName')}
          helperText={`${form.meshName.length}/28 characters`}
          inputProps={{ maxLength: 28 }}
          fullWidth
        />
        <Stack direction="row" spacing={2}>
          <Button variant="contained" onClick={handleSave} disabled={hasErrors}>Save</Button>
          <Button variant="outlined" onClick={handleCancel}>Cancel</Button>
        </Stack>
      </Stack>
    </Box>
  );
}

