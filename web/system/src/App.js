import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import LanguageSwitcher from '../../shared/LanguageSwitcher';
import {
  CssBaseline, AppBar, Toolbar, IconButton, Typography,
  Drawer, List, ListItem, ListItemButton, ListItemText,
  ListSubheader, Divider, Snackbar, Box, Alert,
} from '@mui/material';
import { ThemeProvider, createTheme } from '@mui/material/styles';
import MenuIcon from '@mui/icons-material/Menu';
import MenuOpenIcon from '@mui/icons-material/MenuOpen';
import BatteryFullIcon from '@mui/icons-material/BatteryFull';
import Battery6BarIcon from '@mui/icons-material/Battery6Bar';
import Battery5BarIcon from '@mui/icons-material/Battery5Bar';
import Battery4BarIcon from '@mui/icons-material/Battery4Bar';
import Battery3BarIcon from '@mui/icons-material/Battery3Bar';
import Battery2BarIcon from '@mui/icons-material/Battery2Bar';
import Battery1BarIcon from '@mui/icons-material/Battery1Bar';
import BatteryAlertIcon from '@mui/icons-material/BatteryAlert';

import HomePage from './pages/HomePage';
import WiFiPage from './pages/WiFiPage';
import MeshPage from './pages/MeshPage';
import FilesystemPage from './pages/FilesystemPage';
import DiagnosticsPage from './pages/DiagnosticsPage';

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
    // Default body font: Saira SemiCondensed Light
    fontFamily: "'Saira SemiCondensed', sans-serif",
    fontWeightRegular: 300,
    // Heading variants: Saira Medium
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

function App() {
  const { t } = useTranslation(['system', 'common']);

  // NAV_GROUPS is derived inside the component so labels re-render on language change
  const NAV_GROUPS = [
    { group: t('system:nav.groups.network'), items: [
      { label: t('system:nav.items.wifi'),  page: 'wifi'  },
      { label: t('system:nav.items.mesh'),  page: 'mesh'  },
    ]},
    { group: t('system:nav.groups.otaUpdate'), items: [
      { label: t('system:nav.items.filesystem'), page: 'filesystem' },
    ]},
    { group: t('system:nav.groups.diagnostics'), items: [
      { label: t('system:nav.items.diagnostics'), page: 'diagnostics' },
    ]},
  ];

  const [page, setPage] = useState('home');
  const [drawerOpen, setDrawerOpen] = useState(false);
  const [battery, setBattery] = useState(null);
  const [snackbarOpen, setSnackbarOpen] = useState(false);
  const [snackbarMsg, setSnackbarMsg] = useState('');
  const [snackbarType, setSnackbarType] = useState('info');

  const showSnackbar = (type, msg) => { setSnackbarType(type); setSnackbarMsg(msg); setSnackbarOpen(true); };

  const fetchBattery = async () => {
    try {
      const res = await fetch('/api/battery');
      const data = await res.json();
      const pct = data.battery ?? -1;
      setBattery(pct);
      return pct;
    } catch {
      setBattery(-1);
      return -1;
    }
  };

  // Fetch on mount and poll every 30 seconds
  useEffect(() => {
    fetchBattery();
    const interval = setInterval(fetchBattery, 30000);
    return () => clearInterval(interval);
  }, []);

  const handleBatteryClick = async () => {
    const pct = await fetchBattery();
    if (pct >= 0) {
      showSnackbar('info', t('common:battery.level', { pct }));
    } else {
      showSnackbar('error', t('common:battery.readFailed'));
    }
  };

  const handleNavClick = (p) => { setPage(p); setDrawerOpen(false); };

  const BattIcon = getBatteryIcon(battery);

  const pageMap = {
    home:        <HomePage />,
    wifi:        <WiFiPage onSnackbar={showSnackbar} />,
    mesh:        <MeshPage onSnackbar={showSnackbar} />,
    filesystem:  <FilesystemPage />,
    diagnostics: <DiagnosticsPage onSnackbar={showSnackbar} />,
  };

  return (
    <ThemeProvider theme={theme}>
      <CssBaseline />

      {/* ── App Bar ── */}
      <AppBar position="fixed">
        <Toolbar>
          <IconButton color="inherit" edge="start" onClick={() => setDrawerOpen(true)} sx={{ mr: 1 }}>
            <MenuIcon />
          </IconButton>
          <Typography variant="h6" sx={{ flexGrow: 1, textAlign: 'center' }}>
            {t('system:appBar.title')}
          </Typography>
          <IconButton color="inherit" edge="end" onClick={handleBatteryClick}>
            <BattIcon />
          </IconButton>
        </Toolbar>
      </AppBar>

      {/* ── Navigation Drawer ── */}
      <Drawer
        anchor="left"
        open={drawerOpen}
        onClose={() => setDrawerOpen(false)}
        sx={{ '& .MuiDrawer-paper': { width: '240px', display: 'flex', flexDirection: 'column' } }}
      >
        <Box sx={{ display: 'flex', alignItems: 'center', px: 1, py: 0.5 }}>
          <IconButton onClick={() => setDrawerOpen(false)}>
            <MenuOpenIcon />
          </IconButton>
          <Typography variant="subtitle1" sx={{ flexGrow: 1, textAlign: 'center', pr: 5 }}>
            {t('system:drawer.navigation')}
          </Typography>
        </Box>
        <Divider />
        <List disablePadding>
          {NAV_GROUPS.map(({ group, items }) => (
            <React.Fragment key={group}>
              <ListSubheader>{group}</ListSubheader>
              {items.map(({ label, page: p }) => (
                <ListItem key={p} disablePadding>
                  <ListItemButton selected={page === p} onClick={() => handleNavClick(p)} sx={{ pl: 4 }}>
                    <ListItemText primary={label} />
                  </ListItemButton>
                </ListItem>
              ))}
            </React.Fragment>
          ))}
        </List>
        <Box sx={{ mt: 'auto', mb: 2, px: 2, pt: 2, borderTop: 1, borderColor: 'divider' }}>
          <LanguageSwitcher />
        </Box>
      </Drawer>

      {/* ── Page Content ── */}
      <Box component="main" sx={{ pt: 8 }}>
        {pageMap[page]}
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

