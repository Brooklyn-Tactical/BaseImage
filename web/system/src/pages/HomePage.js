import React, { useState, useEffect } from 'react';
import { Box, Typography, Divider } from '@mui/material';
import { useTranslation } from 'react-i18next';

export default function HomePage() {
  const { t } = useTranslation(['system', 'common']);
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
        {t('system:home.productName')}
      </Typography>
      <Typography variant="h6" align="center" color="text.secondary" gutterBottom>
        {t('system:home.by')}
      </Typography>
      <Typography variant="h4" align="center" fontWeight="bold" gutterBottom>
        {t('system:home.brand')}
      </Typography>

      <Box
        component="img"
        src="/shared/BT-logo.jpg"
        alt={t('system:home.logoAlt')}
        sx={{ width: '100%', maxWidth: 360, my: 2 }}
      />

      <Typography variant="body1" align="center">
        {t('common:versions.hw', { version: hw })}
      </Typography>
      <Typography variant="body1" align="center">
        {t('common:versions.sw', { version: fw })}
      </Typography>
      <Typography variant="body1" align="center">
        {t('common:versions.fsShort', { version: fs })}
      </Typography>
    </Box>
  );
}

