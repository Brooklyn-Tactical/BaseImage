import React, { useState, useEffect } from 'react';
import {
  CssBaseline, AppBar, Toolbar, IconButton, Typography,
  Drawer, Divider, Snackbar, Box, Button, Stack, TextField,
} from '@mui/material';
import { ThemeProvider, createTheme } from '@mui/material/styles';
import InfoOutlinedIcon from '@mui/icons-material/InfoOutlined';
import MenuOpenIcon from '@mui/icons-material/MenuOpen';
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

const DRAWER_WIDTH = 240;

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

function App() {
  const [drawerOpen, setDrawerOpen] = useState(false);
  const [battery, setBattery]       = useState(null);
  const [snackbarOpen, setSnackbarOpen] = useState(false);
  const [snackbarMsg, setSnackbarMsg]   = useState('');
  const [versions, setVersions]     = useState(null);
  const [gameState, setGameState]   = useState(null);
  const [score, setScore]           = useState(0);
  const [gameTime, setGameTime]     = useState(0);
  const [loading, setLoading]       = useState(true);

  // Game duration — committed value in seconds; durMins/durSecs are the live input fields
  const [durationSecs, setDurationSecs] = useState(60);
  const [durMins, setDurMins]           = useState(1);
  const [durSecs, setDurSecs]           = useState(0);

  const showSnackbar = (msg) => { setSnackbarMsg(msg); setSnackbarOpen(true); };

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
    if (pct >= 0) showSnackbar(`Battery Level: ${pct}%`);
    else          showSnackbar('Failed to read battery level');
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
      setScore(data.score);
      setGameTime(data.gameTime ?? 0);
      if (data.duration != null) {
        const d = Math.max(1, data.duration);
        setDurationSecs(d);
        setDurMins(Math.floor(d / 60));
        setDurSecs(d % 60);
      }
    } catch (error) {
      console.error('Error fetching game state:', error);
    } finally {
      setLoading(false);
    }
  };

  // Called when either the minutes or seconds input changes.
  // newMins / newSecs are the candidate values; the other field keeps its current value.
  const handleDurationChange = async (newMins, newSecs) => {
    const total = Math.max(0, newMins) * 60 + Math.max(0, newSecs);
    // Optimistically update the display
    setDurMins(newMins);
    setDurSecs(newSecs);
    try {
      const response = await fetch('/api/game/duration', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ duration: total }),
      });
      const data = await response.json();
      if (data.success) {
        // Commit confirmed value from server (server is the source of truth)
        const confirmed = data.duration;
        setDurationSecs(confirmed);
        setDurMins(Math.floor(confirmed / 60));
        setDurSecs(confirmed % 60);
      } else {
        // Roll back to last confirmed value
        setDurMins(Math.floor(durationSecs / 60));
        setDurSecs(durationSecs % 60);
      }
    } catch {
      // Network error — roll back
      setDurMins(Math.floor(durationSecs / 60));
      setDurSecs(durationSecs % 60);
    }
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

  const handlePauseResume = async () => {
    try {
      const response = await fetch('/api/game/pauseResume', { method: 'POST' });
      const data = await response.json();
      if (data.success) setGameState(data.state);
    } catch (error) {
      console.error('Error toggling pause/resume:', error);
    }
  };

  const handleStartGame = async () => {
    try {
      const response = await fetch('/api/game/state', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ state: 'playing' }),
      });
      const data = await response.json();
      if (data.success) setGameState('playing');
    } catch (error) {
      console.error('Error starting game:', error);
    }
  };

  const handleResetGame = async () => {
    try {
      const response = await fetch('/api/game/reset', { method: 'POST' });
      const data = await response.json();
      if (data.success) { setGameState(data.state); setScore(data.score); }
    } catch (error) {
      console.error('Error resetting game:', error);
    }
  };

  const handleAddPoints = async () => {
    try {
      const response = await fetch('/api/game/addPoints', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ points: 10 }),
      });
      const data = await response.json();
      if (data.success) setScore(data.score);
    } catch (error) {
      console.error('Error updating score:', error);
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
            Game Prop
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
        sx={{ '& .MuiDrawer-paper': { width: DRAWER_WIDTH } }}
      >
        <Box sx={{ display: 'flex', alignItems: 'center', px: 1, py: 0.5 }}>
          <IconButton onClick={() => setDrawerOpen(false)}>
            <MenuOpenIcon />
          </IconButton>
          <Typography variant="subtitle1" sx={{ flexGrow: 1, textAlign: 'center', pr: 5 }}>
            Admin Access
          </Typography>
        </Box>
        <Divider />
        <Box sx={{ p: 2 }}>
          <Typography variant="body2" color="text.secondary">
            To access the administrative System Mode press the "mode select" button on your prop within 5 seconds of
            power up.  Once the prop enters System Mode it will create a wifi access point called
            "BrooklynTactical-<i>XYZ</i>" that can be accessed without a password.  System mode allows configuration of:
            <ul>
              <li>Wifi network</li>
              <li>Mesh networking</li>
              <li>Firmware/Filesystem</li>
            </ul>
          </Typography>
        </Box>
      </Drawer>

      {/* ── Main Content ── */}
      <Box component="main" sx={{ pt: 8 }}>

        {/* Versions section — from HomePage */}
        <Box sx={{ display: 'flex', flexDirection: 'column', alignItems: 'center', mt: 4, px: 2 }}>
          <Typography variant="h4" align="center" fontWeight="bold" gutterBottom>
            Game Prop
          </Typography>
          <Typography variant="h6" align="center" color="text.secondary" gutterBottom>
            by
          </Typography>
          <Typography variant="h4" align="center" fontWeight="bold" gutterBottom>
            Brooklyn Tactical
          </Typography>
          <Divider sx={{ width: '100%', maxWidth: 360, my: 3 }} />
          <Typography variant="body1" align="center">H/W Version: {hw}</Typography>
          <Typography variant="body1" align="center">S/W Version: {fw}</Typography>
          <Typography variant="body1" align="center">FS Version: {fs}</Typography>
        </Box>

        <Divider sx={{ maxWidth: 360, mx: 'auto', my: 4 }} />

        {/* Game section */}
        <Box sx={{ display: 'flex', flexDirection: 'column', alignItems: 'center', px: 2, pb: 4 }}>
          {loading ? (
            <Typography variant="h6">Loading…</Typography>
          ) : (
            <>
              <Box sx={{ mb: 3, textAlign: 'center' }}>
                <Typography variant="body1"><strong>State:</strong> {gameState || 'idle'}</Typography>
                <Typography variant="body1"><strong>Score:</strong> {score}</Typography>
                <Typography variant="body1"><strong>Game Time:</strong> {formatGameTime(gameTime)}</Typography>
              </Box>

              {/* Game Duration configuration — editable only in idle/configure state */}
              <Box sx={{ mb: 3, display: 'flex', alignItems: 'center', gap: 1 }}>
                <Typography variant="body1" sx={{ mr: 1 }}><strong>Game Duration:</strong></Typography>
                <TextField
                  type="number"
                  label="mm"
                  value={durMins}
                  disabled={gameState !== 'idle' && gameState !== null}
                  onChange={(e) => handleDurationChange(Math.max(0, parseInt(e.target.value, 10) || 0), durSecs)}
                  inputProps={{ min: 0, max: 99, step: 1 }}
                  sx={{ width: 72 }}
                  size="small"
                />
                <Typography variant="body1">:</Typography>
                <TextField
                  type="number"
                  label="ss"
                  value={durSecs}
                  disabled={gameState !== 'idle' && gameState !== null}
                  onChange={(e) => handleDurationChange(durMins, Math.min(59, Math.max(0, parseInt(e.target.value, 10) || 0)))}
                  inputProps={{ min: 0, max: 59, step: 1 }}
                  sx={{ width: 72 }}
                  size="small"
                />
              </Box>
              <Stack spacing={1.5} sx={{ width: '100%', maxWidth: 320 }}>
                <Button
                  variant="contained"
                  fullWidth
                  onClick={handlePauseResume}
                  disabled={gameState !== 'playing' && gameState !== 'paused'}
                >
                  {gameState === 'paused' ? 'Resume' : 'Pause'}
                </Button>
                <Button variant="contained" color="success" fullWidth onClick={handleStartGame}>
                  Start Game
                </Button>
                <Button variant="contained" color="warning" fullWidth onClick={handleResetGame}>
                  Reset Game
                </Button>
                <Button variant="outlined" fullWidth onClick={handleAddPoints}>
                  Add 10 Points
                </Button>
              </Stack>
            </>
          )}
        </Box>
      </Box>

      {/* ── Battery Snackbar ── */}
      <Snackbar
        open={snackbarOpen}
        autoHideDuration={5000}
        onClose={() => setSnackbarOpen(false)}
        message={snackbarMsg}
        anchorOrigin={{ vertical: 'bottom', horizontal: 'center' }}
      />
    </ThemeProvider>
  );
}

export default App;

