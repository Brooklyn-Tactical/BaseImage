import React, { useState, useEffect } from 'react';
import { Box, Typography, Divider } from '@mui/material';

export default function HomePage() {
  const [versions, setVersions] = useState(null);

  useEffect(() => {
    fetch('/api/versions')
      .then(r => r.json())
      .then(data => setVersions(data))
      .catch(() => setVersions({ hw: 'error', fw: 'error', fs: 'error' }));
  }, []);

  const hw = versions ? versions.hw : '…';
  const fw = versions ? versions.fw : '…';
  const fs = versions ? versions.fs : '…';

  return (
    <Box
      sx={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        mt: 8,
        px: 2,
      }}
    >
      <Typography variant="h4" align="center" fontWeight="bold" gutterBottom>
        Device Name
      </Typography>
      <Typography variant="h6" align="center" color="text.secondary" gutterBottom>
        by
      </Typography>
      <Typography variant="h4" align="center" fontWeight="bold" gutterBottom>
        Brooklyn Tactical
      </Typography>

      <Divider sx={{ width: '100%', maxWidth: 360, my: 3 }} />

      <Typography variant="body1" align="center">
        H/W Version: {hw}
      </Typography>
      <Typography variant="body1" align="center">
        S/W Version: {fw}
      </Typography>
      <Typography variant="body1" align="center">
        FS Version: {fs}
      </Typography>
    </Box>
  );
}

