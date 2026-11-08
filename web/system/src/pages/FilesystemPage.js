import React, { useState, useRef } from 'react';
import {
  Box, Typography, Stack, Button,
  Dialog, DialogTitle, DialogContent, DialogContentText, DialogActions,
  Paper,
} from '@mui/material';
import { useTranslation } from 'react-i18next';
import i18n from '../../../shared/i18n';



// ── localStorage key prefix for lifted files ─────────────────────────────
const LS_PREFIX = 'lifted:';

// ── Lift config files from device into localStorage ───────────────────────
async function liftConfig(log) {
  const t = (key, opts) => i18n.t(key, opts);
  log(t('system:filesystem.log.fetchingFiles'));
  const manifestRes = await fetch('/api/system/files');
  if (!manifestRes.ok) throw new Error(t('system:filesystem.log.manifestError'));
  const { lifted_files } = await manifestRes.json();
  log(lifted_files.length > 0
    ? t('system:filesystem.log.filesToPreserve', { files: lifted_files.join(', ') })
    : t('system:filesystem.log.filesToPreserveNone'));

  log(t('system:filesystem.log.savingToStorage'));
  let allSaved = true;
  for (const name of lifted_files) {
    const path = name.startsWith('/') ? name : '/' + name;
    const res = await fetch(`/api/system/file?path=${encodeURIComponent(path)}`);
    if (res.ok) {
      localStorage.setItem(LS_PREFIX + path, await res.text());
      log(t('system:filesystem.log.savedFile', { path }));
    } else {
      log(t('system:filesystem.log.skippedFile', { path }));
      allSaved = false;
    }
  }
  return allSaved;
}

// ── Restore files already in localStorage (also used as stand-alone retry) ─
async function restoreLifted(log) {
  const t = (key, opts) => i18n.t(key, opts);
  const keys = Object.keys(localStorage).filter(k => k.startsWith(LS_PREFIX));
  if (keys.length === 0) { log(t('system:filesystem.log.noFilesToRestore')); return true; }

  log(t('system:filesystem.log.restoring', { count: keys.length }));
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
        log(t('system:filesystem.log.restoredFile', { path }));
      } else {
        log(t('system:filesystem.log.restoreFileFailed', { path }));
        allOk = false;
      }
    } catch {
      log(t('system:filesystem.log.restoreFileNetError', { path }));
      allOk = false;
    }
  }
  return allOk;
}

// ─────────────────────────────────────────────────────────────────────────────

export default function FilesystemPage() {
  const { t } = useTranslation(['system', 'common']);

  const [liftOverwriteDialogOpen, setLiftOverwriteDialogOpen] = useState(false);
  const [activeAction, setActiveAction]                       = useState(null); // 'lift' | 'restore'
  const [phase, setPhase]                                     = useState('idle'); // idle|running|done|error
  const [statusLines, setStatusLines]                         = useState([]);
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

  // ── Lift handlers ─────────────────────────────────────────────────────────
  const startLift = () => {
    if (hasPendingRestore) {
      setLiftOverwriteDialogOpen(true);
    } else {
      executeLift();
    }
  };

  const executeLift = async () => {
    setLiftOverwriteDialogOpen(false);
    setActiveAction('lift');
    setPhase('running');
    setStatusLines([]);
    try {
      const allSaved = await liftConfig(appendLog);
      setPhase(allSaved ? 'done' : 'error');
      appendLog(allSaved
        ? i18n.t('system:filesystem.log.liftComplete')
        : i18n.t('system:filesystem.log.liftPartial'));
    } catch (err) {
      appendLog(i18n.t('system:filesystem.log.error', { message: err.message }));
      setPhase('error');
    }
  };

  // ── Restore handler ───────────────────────────────────────────────────────
  const startRestore = async () => {
    setActiveAction('restore');
    setPhase('running');
    setStatusLines([]);
    try {
      const allOk = await restoreLifted(appendLog);
      setPhase(allOk ? 'done' : 'error');
      appendLog(allOk
        ? i18n.t('system:filesystem.log.restoreComplete')
        : i18n.t('system:filesystem.log.restorePartial'));
    } catch (err) {
      appendLog(i18n.t('system:filesystem.log.error', { message: err.message }));
      setPhase('error');
    }
  };

  const isRunning = phase === 'running';

  const statusKey =
    phase === 'done'  && activeAction === 'lift'    ? 'system:filesystem.status.liftDone'    :
    phase === 'error' && activeAction === 'lift'    ? 'system:filesystem.status.liftError'   :
    phase === 'done'  && activeAction === 'restore' ? 'system:filesystem.status.restoreDone' :
    phase === 'error' && activeAction === 'restore' ? 'system:filesystem.status.restoreError':
    null;

  return (
    <Box sx={{ p: 3 }}>
      <Typography variant="h5" gutterBottom>{t('system:filesystem.title')}</Typography>
      <Typography variant="body1" color="text.secondary"
        sx={{ mb: 2, maxWidth: 520 }}>
        {t('system:filesystem.warning')}
      </Typography>
      <Typography variant="body1" sx={{ mb: 3, maxWidth: 520 }}>{t('system:filesystem.captiveNote')}</Typography>

      <Stack spacing={2} sx={{ maxWidth: 400 }}>
        <Button variant="contained" fullWidth
          disabled={isRunning}
          onClick={startLift}>
          {t('system:filesystem.liftButton')}
        </Button>

        <Button variant="contained" color="success" fullWidth
          disabled={!hasPendingRestore || isRunning}
          onClick={startRestore}>
          {t('system:filesystem.restoreButton')}
        </Button>
      </Stack>

      {phase === 'done' && activeAction === 'lift' && (
        <Typography color="info.main" sx={{ mt: 2, maxWidth: 520 }}>
          {t('system:filesystem.nextSteps')}
        </Typography>
      )}

      {statusLines.length > 0 && (
        <Paper variant="outlined" ref={logBoxRef}
          sx={{ mt: 3, p: 1.5, maxWidth: 520, maxHeight: 220, overflowY: 'auto',
                fontFamily: 'monospace', fontSize: '0.78rem', whiteSpace: 'pre-wrap' }}>
          {statusLines.map((l, i) => <div key={i}>{l}</div>)}
        </Paper>
      )}

      {statusKey && (
        <Typography
          color={phase === 'done' ? 'success.main' : 'error.main'}
          sx={{ mt: 2, fontWeight: 600 }}>
          {t(statusKey)}
        </Typography>
      )}

      <Dialog open={liftOverwriteDialogOpen} onClose={() => !isRunning && setLiftOverwriteDialogOpen(false)}>
        <DialogTitle>{t('system:filesystem.dialog.title')}</DialogTitle>
        <DialogContent>
          <DialogContentText>
            {t('system:filesystem.dialog.body')}
          </DialogContentText>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setLiftOverwriteDialogOpen(false)}>{t('common:action.cancel')}</Button>
          <Button variant="contained" color="warning" onClick={executeLift}>
            {t('common:action.proceed')}
          </Button>
        </DialogActions>
      </Dialog>
    </Box>
  );
}

