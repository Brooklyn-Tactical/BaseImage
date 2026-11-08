import React, { useState, useEffect, useRef } from 'react';
import { useTranslation, Trans } from 'react-i18next';
import LanguageSwitcher from '../../shared/LanguageSwitcher';
import {
  CssBaseline, AppBar, Toolbar, IconButton, Typography,
  Drawer, Divider, Snackbar, Box, Button, Stack, TextField,
  Card, CardContent, Collapse, Switch, InputAdornment, Alert,
} from '@mui/material';
import { ThemeProvider, createTheme } from '@mui/material/styles';
import InfoOutlinedIcon from '@mui/icons-material/InfoOutlined';
import MenuOpenIcon from '@mui/icons-material/MenuOpen';
import ArrowDropDownIcon from '@mui/icons-material/ArrowDropDown';
import BatteryFullIcon from '@mui/icons-material/BatteryFull';
import Battery6BarIcon from '@mui/icons-material/Battery6Bar';
import Battery5BarIcon from '@mui/icons-material/Battery5Bar';
import Battery4BarIcon from '@mui/icons-material/Battery4Bar';
import Battery3BarIcon from '@mui/icons-material/Battery3Bar';
import Battery2BarIcon from '@mui/icons-material/Battery2Bar';
import Battery1BarIcon from '@mui/icons-material/Battery1Bar';
import BatteryAlertIcon from '@mui/icons-material/BatteryAlert';

const BATTERY_ICONS = [
  BatteryFullIcon, Battery6BarIcon, Battery5BarIcon, Battery4BarIcon,
  Battery3BarIcon, Battery2BarIcon, Battery1BarIcon, BatteryAlertIcon,
];

function getBatteryIcon(pct) {
  const clamped = Math.max(0, Math.min(100, pct ?? 100));
  const idx = Math.min(7, Math.floor((100 - clamped) / 12.5));
  return BATTERY_ICONS[idx];
}

const theme = createTheme({
  typography: {
    fontFamily: "'Saira SemiCondensed', sans-serif",
    fontWeightRegular: 300,
    h1: { fontFamily: "'Saira', sans-serif", fontWeight: 500 },
    h2: { fontFamily: "'Saira', sans-serif", fontWeight: 500 },
    h3: { fontFamily: "'Saira', sans-serif", fontWeight: 500 },
    h4: { fontFamily: "'Saira', sans-serif", fontWeight: 500 },
    h5: { fontFamily: "'Saira', sans-serif", fontWeight: 500 },
    h6: { fontFamily: "'Saira', sans-serif", fontWeight: 500 },
    subtitle1: { fontFamily: "'Saira', sans-serif", fontWeight: 500 },
    subtitle2: { fontFamily: "'Saira', sans-serif", fontWeight: 500 },
  },
});

// Converts whole seconds from the firmware into hh:mm:ss (or mm:ss when < 1 hour)
function formatGameTime(totalSeconds) {
  const s = Math.max(0, Math.floor(totalSeconds));
  const hh = Math.floor(s / 3600);
  const mm = Math.floor((s % 3600) / 60);
  const ss = s % 60;
  const pad = (n) => String(n).padStart(2, '0');
  return hh > 0
    ? `${pad(hh)}:${pad(mm)}:${pad(ss)}`
    : `${pad(mm)}:${pad(ss)}`;
}

// ── SettingRow — label + control + collapsible description ────────────────────
function SettingRow({ label, description, expanded, onToggle, children }) {
  return (
    <Box>
      <Box sx={{ display: 'flex', alignItems: 'center', minHeight: 44, gap: 1 }}>
        <Typography variant="body2" sx={{ flexGrow: 1 }}>{label}</Typography>
        {children}
        <IconButton size="small" onClick={onToggle} sx={{ p: 0.5 }}>
          <ArrowDropDownIcon sx={{
            transform: expanded ? 'rotate(180deg)' : 'rotate(0deg)',
            transition: 'transform 0.2s',
          }} />
        </IconButton>
      </Box>
      <Collapse in={expanded}>
        <Typography variant="caption" color="text.secondary"
          sx={{ display: 'block', px: 1, pb: 1.5, lineHeight: 1.5 }}>
          {description}
        </Typography>
      </Collapse>
    </Box>
  );
}

function App() {
  const { t } = useTranslation(['game', 'common']);

  const [drawerOpen, setDrawerOpen] = useState(false);
  const [battery, setBattery]       = useState(null);
  const [snackbarOpen, setSnackbarOpen] = useState(false);
  const [snackbarMsg, setSnackbarMsg]   = useState('');
  const [snackbarType, setSnackbarType] = useState('info');
  const [versions, setVersions]     = useState(null);
  const [description, setDescription] = useState('');
  const [gameState, setGameState]   = useState(null);
  const [gameTime, setGameTime]     = useState(0);
  const [loading, setLoading]       = useState(true);

  // ── Settings state ──────────────────────────────────────────────────────────
  const [countdownMins,   setCountdownMins]   = useState(5);
  const [armDisarmSecs,   setArmDisarmSecs]   = useState(10);
  const [multipleArmings, setMultipleArmings] = useState(false);
  const [standbyMins,     setStandbyMins]     = useState(30);
  const [showStandbyTime, setShowStandbyTime] = useState(false);

  // Which description panels are open
  const [expCountdown,       setExpCountdown]       = useState(false);
  const [expArmDisarm,       setExpArmDisarm]        = useState(false);
  const [expMultipleArmings, setExpMultipleArmings] = useState(false);
  const [expStandby,         setExpStandby]          = useState(false);
  const [expShowStandby,     setExpShowStandby]      = useState(false);

  // Populated once from the first successful /api/game/state fetch; subsequent
  // polls update gameState/gameTime/description but leave user-editable settings alone.
  const settingsInitialized = useRef(false);

  const showSnackbar = (type, msg) => { setSnackbarType(type); setSnackbarMsg(msg); setSnackbarOpen(true); };

  // ── Battery ──────────────────────────────────────────────────────────────
  const fetchBattery = async () => {
    try {
      const res  = await fetch('/api/battery');
      const data = await res.json();
      const pct  = data.battery ?? -1;
      setBattery(pct);
      return pct;
    } catch {
      setBattery(-1);
      return -1;
    }
  };

  const handleBatteryClick = async () => {
    const pct = await fetchBattery();
    if (pct >= 0) showSnackbar('info', t('common:battery.level', { pct }));
    else          showSnackbar('error', t('common:battery.readFailed'));
  };

  // ── Versions ─────────────────────────────────────────────────────────────
  const fetchVersions = () =>
    fetch('/api/versions')
      .then(r => r.json())
      .then(data => setVersions(data))
      .catch(() => setVersions({ hw: 'error', fw: 'error', fs: 'error' }));

  // ── Game state ────────────────────────────────────────────────────────────
  const fetchGameState = async () => {
    try {
      const response = await fetch('/api/game/state');
      const data = await response.json();
      setGameState(data.state);
      setGameTime(data.gameTime ?? 0);
      if (data.description != null) setDescription(data.description);
      // Populate editable settings only on the first successful fetch so that
      // subsequent 5-second polls do not overwrite values the user has changed.
      if (!settingsInitialized.current) {
        if (data.countdownSeconds != null)
          setCountdownMins(Math.max(1, Math.round(data.countdownSeconds / 60)));
        if (data.armDisarmSeconds  != null) setArmDisarmSecs(data.armDisarmSeconds);
        if (data.multipleArmings   != null) setMultipleArmings(data.multipleArmings);
        if (data.standbyMinutes    != null) setStandbyMins(data.standbyMinutes);
        if (data.showStandbyTime   != null) setShowStandbyTime(data.showStandbyTime);
        settingsInitialized.current = true;
      }
    } catch (error) {
      console.error('Error fetching game state:', error);
    } finally {
      setLoading(false);
    }
  };


  // ── Settings handlers — update local state only; values are sent on "Start Game" ──
  const handleCountdownMins = (raw) => {
    const val = Math.max(1, Math.min(99, parseInt(raw, 10) || 1));
    setCountdownMins(val);
  };

  const handleArmDisarmSecs = (raw) => {
    const val = Math.max(5, Math.min(99, parseInt(raw, 10) || 5));
    setArmDisarmSecs(val);
  };

  const handleMultipleArmings = (enabled) => {
    setMultipleArmings(enabled);
  };

  const handleStandbyMins = (raw) => {
    const val = Math.max(0, Math.min(199, parseInt(raw, 10) || 0));
    setStandbyMins(val);
  };

  const handleShowStandbyTime = (enabled) => {
    setShowStandbyTime(enabled);
  };

  useEffect(() => {
    fetchBattery();
    fetchVersions();
    fetchGameState();
    const batteryInterval   = setInterval(fetchBattery,   30000);
    const gameStateInterval = setInterval(fetchGameState,  5000);
    return () => {
      clearInterval(batteryInterval);
      clearInterval(gameStateInterval);
    };
  }, []);

  const handleStartGame = async () => {
    try {
      // Send all settings and trigger FSM transition in a single call
      const response = await fetch('/api/game/start', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          countdownSeconds: countdownMins * 60,
          armDisarmSeconds: armDisarmSecs,
          multipleArmings,
          standbyMinutes:   standbyMins,
          showStandbyTime,
        }),
      });
      const data = await response.json();
      if (data.success) {
        setGameState(data.state);
        showSnackbar('success', t('game:snackbar.gameStarted'));
      }
    } catch (error) {
      console.error('Error starting game:', error);
      showSnackbar('error', t('game:snackbar.gameStartError'));
    }
  };

  const handleSaveDefaults = async () => {
    try {
      const response = await fetch('/api/game/defaults', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          countdownMins,
          armDisarmSecs,
          multipleArmings,
          standbyMins,
          showStandbyTime,
        }),
      });
      const data = await response.json();
      if (data.success !== false) showSnackbar('success', t('game:snackbar.defaultsSaved'));
      else showSnackbar('error', t('game:snackbar.defaultsSaveFailed'));
    } catch (error) {
      console.error('Error saving defaults:', error);
      showSnackbar('error', t('game:snackbar.defaultsSaveFailed'));
    }
  };

  const handleLoadDefaults = async () => {
    try {
      const response = await fetch('/api/game/defaults');
      if (!response.ok) {
        showSnackbar('info', t('game:snackbar.noSavedDefaults'));
        return;
      }
      const data = await response.json();
      if (data.countdownMins   != null) handleCountdownMins(data.countdownMins);
      if (data.armDisarmSecs   != null) handleArmDisarmSecs(data.armDisarmSecs);
      if (data.multipleArmings != null) handleMultipleArmings(data.multipleArmings);
      if (data.standbyMins     != null) handleStandbyMins(data.standbyMins);
      if (data.showStandbyTime != null) handleShowStandbyTime(data.showStandbyTime);
      showSnackbar('success', t('game:snackbar.defaultsLoaded'));
    } catch (error) {
      console.error('Error loading defaults:', error);
      showSnackbar('error', t('game:snackbar.defaultsLoadFailed'));
    }
  };

  const BattIcon = getBatteryIcon(battery);
  const hw = versions ? versions.hw : '…';
  const fw = versions ? versions.fw : '…';
  const fs = versions ? versions.fs : '…';

  return (
    <ThemeProvider theme={theme}>
      <CssBaseline />

      {/* ── App Bar ── */}
      <AppBar position="fixed">
        <Toolbar>
          <IconButton color="inherit" edge="start" onClick={() => setDrawerOpen(true)} sx={{ mr: 1 }}>
            <InfoOutlinedIcon />
          </IconButton>
          <Typography variant="h6" sx={{ flexGrow: 1, textAlign: 'center' }}>
            {description || t('game:appBar.defaultTitle')}
          </Typography>
          <IconButton color="inherit" edge="end" onClick={handleBatteryClick}>
            <BattIcon />
          </IconButton>
        </Toolbar>
      </AppBar>

      {/* ── Info Drawer ── */}
      <Drawer
        anchor="left"
        open={drawerOpen}
        onClose={() => setDrawerOpen(false)}
        sx={{ '& .MuiDrawer-paper': { width: '240px' } }}
      >
        <Box sx={{ display: 'flex', alignItems: 'center', px: 1, py: 0.5 }}>
          <IconButton onClick={() => setDrawerOpen(false)}>
            <MenuOpenIcon />
          </IconButton>
          <Typography variant="subtitle1" sx={{ flexGrow: 1, textAlign: 'center', pr: 5 }}>
            {t('game:drawer.title')}
          </Typography>
        </Box>
        <Divider />
        <Box sx={{ p: 2 }}>
          <Typography variant="body2" color="text.secondary">
            <Trans
              i18nKey="game:drawer.adminBody"
              components={{ em: <i />, ul: <ul />, li: <li /> }}
            />
          </Typography>
        </Box>
        <Box sx={{ mt: 'auto', p: 2, borderTop: 1, borderColor: 'divider' }}>
          <LanguageSwitcher sx={{ mb: 2 }} />
          <Box
              component="img"
              src="/shared/BT-logo.jpg"
              alt={t('game:drawer.logoAlt')}
              sx={{ width: '100%', maxWidth: 360, my: 2 }}
          />
          <Typography variant="caption" color="text.secondary" display="block" align="center">
            {t('common:versions.hw', { version: hw })}
          </Typography>
          <Typography variant="caption" color="text.secondary" display="block" align="center">
            {t('common:versions.sw', { version: fw })}
          </Typography>
          <Typography variant="caption" color="text.secondary" display="block" align="center">
            {t('common:versions.fs', { version: fs })}
          </Typography>
        </Box>
      </Drawer>

      {/* ── Main Content ── */}
      <Box component="main" sx={{ pt: 8 }}>

        {/* Versions section — from HomePage */}
        <Box sx={{ display: 'flex', flexDirection: 'column', alignItems: 'center', mt: 4, px: 2 }}>
          <Typography variant="h4" align="center" fontWeight="bold" gutterBottom>
            {t('game:hero.productName')}
          </Typography>
          <Typography variant="h6" align="center" color="text.secondary" gutterBottom>
            {t('game:hero.by')}
          </Typography>
          <Typography variant="h4" align="center" fontWeight="bold" gutterBottom>
            {t('game:hero.brand')}
          </Typography>
        </Box>

        <Divider sx={{ maxWidth: 360, mx: 'auto', my: 4 }} />

        {/* Game section */}
        <Box sx={{ display: 'flex', flexDirection: 'column', alignItems: 'center', px: 2, pb: 4 }}>
          {loading ? (
            <Typography variant="h6">{t('game:status.loading')}</Typography>
          ) : (
            <>
              <Box sx={{ mb: 3, textAlign: 'center' }}>
                <Typography variant="body1"><strong>{t('game:status.state')}</strong>{' '}{gameState || 'configure'}</Typography>
                {['await_arming', 'arming', 'armed', 'countdown', 'disarming', 'disarmed'].includes(gameState) && (
                  <Typography variant="body1"><strong>{t('game:status.gameTime')}</strong>{' '}{formatGameTime(gameTime)}</Typography>
                )}
              </Box>

              {/* ── Game Settings Card ── */}
              <Card variant="outlined" sx={{ width: '100%', maxWidth: 360, mb: 3 }}>
                <CardContent sx={{ pb: '12px !important' }}>
                  <Typography variant="subtitle2" sx={{ mb: 1 }}>{t('game:settings.title')}</Typography>
                  <Divider sx={{ mb: 0.5 }} />
                  <SettingRow
                    label={t('game:settings.countdown.label')}
                    description={t('game:settings.countdown.description')}
                    expanded={expCountdown}
                    onToggle={() => setExpCountdown(v => !v)}
                  >
                    <TextField
                      type="number"
                      value={countdownMins}
                      onChange={(e) => setCountdownMins(e.target.value)}
                      onBlur={(e) => handleCountdownMins(e.target.value)}
                      inputProps={{ min: 1, max: 99, step: 1 }}
                      InputProps={{ endAdornment: <InputAdornment position="end">{t('game:settings.countdown.unit')}</InputAdornment> }}
                      sx={{ width: 105 }}
                      size="small"
                    />
                  </SettingRow>
                  <Divider sx={{ my: 0.5 }} />
                  <SettingRow
                    label={t('game:settings.armDisarm.label')}
                    description={t('game:settings.armDisarm.description')}
                    expanded={expArmDisarm}
                    onToggle={() => setExpArmDisarm(v => !v)}
                  >
                    <TextField
                      type="number"
                      value={armDisarmSecs}
                      onChange={(e) => setArmDisarmSecs(e.target.value)}
                      onBlur={(e) => handleArmDisarmSecs(e.target.value)}
                      inputProps={{ min: 5, max: 99, step: 1 }}
                      InputProps={{ endAdornment: <InputAdornment position="end">{t('game:settings.armDisarm.unit')}</InputAdornment> }}
                      sx={{ width: 105 }}
                      size="small"
                    />
                  </SettingRow>
                  <Divider sx={{ my: 0.5 }} />
                  <SettingRow
                    label={t('game:settings.multipleArmings.label')}
                    description={t('game:settings.multipleArmings.description')}
                    expanded={expMultipleArmings}
                    onToggle={() => setExpMultipleArmings(v => !v)}
                  >
                    <Switch
                      checked={multipleArmings}
                      onChange={(e) => handleMultipleArmings(e.target.checked)}
                      size="small"
                    />
                  </SettingRow>
                  <Divider sx={{ my: 0.5 }} />
                  <SettingRow
                    label={t('game:settings.standby.label')}
                    description={t('game:settings.standby.description')}
                    expanded={expStandby}
                    onToggle={() => setExpStandby(v => !v)}
                  >
                    <TextField
                      type="number"
                      value={standbyMins}
                      onChange={(e) => setStandbyMins(e.target.value)}
                      onBlur={(e) => handleStandbyMins(e.target.value)}
                      inputProps={{ min: 0, max: 199, step: 1 }}
                      InputProps={{ endAdornment: <InputAdornment position="end">{t('game:settings.standby.unit')}</InputAdornment> }}
                      sx={{ width: 105 }}
                      size="small"
                    />
                  </SettingRow>
                  <Divider sx={{ my: 0.5 }} />
                  <SettingRow
                    label={t('game:settings.showStandby.label')}
                    description={t('game:settings.showStandby.description')}
                    expanded={expShowStandby}
                    onToggle={() => setExpShowStandby(v => !v)}
                  >
                    <Switch
                      checked={showStandbyTime}
                      onChange={(e) => handleShowStandbyTime(e.target.checked)}
                      size="small"
                    />
                  </SettingRow>
                </CardContent>
              </Card>

              {/* ── Game control buttons ── */}
              <Stack spacing={1.5} sx={{ width: '100%', maxWidth: 360 }}>
                <Box sx={{ display: 'flex', gap: 1 }}>
                  <Button
                    variant="outlined"
                    sx={{ flex: 1 }}
                    onClick={handleLoadDefaults}
                  >
                    {t('common:action.load')}
                  </Button>
                  <Button
                    variant="outlined"
                    sx={{ flex: 1 }}
                    onClick={handleSaveDefaults}
                  >
                    {t('common:action.save')}
                  </Button>
                </Box>
                <Button
                  variant="contained"
                  color="success"
                  fullWidth
                  disabled={gameState === 'lowbattery'}
                  onClick={handleStartGame}
                >
                  {gameState === 'configure' || !gameState ? t('game:control.startGame') : t('game:control.restartGame')}
                </Button>
              </Stack>
            </>
          )}
        </Box>
      </Box>

      {/* ── Snackbar ── */}
      <Snackbar
        open={snackbarOpen}
        autoHideDuration={5000}
        onClose={() => setSnackbarOpen(false)}
        anchorOrigin={{ vertical: 'bottom', horizontal: 'center' }}
      >
        <Alert onClose={() => setSnackbarOpen(false)} severity={snackbarType} sx={{ width: '100%', border: '1px solid black' }}>
          {snackbarMsg}
        </Alert>
      </Snackbar>
    </ThemeProvider>
  );
}

export default App;

