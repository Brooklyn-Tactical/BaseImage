import React from 'react';
import {
  FormControl, InputLabel, Select, MenuItem,
} from '@mui/material';
import { useTranslation } from 'react-i18next';
import i18n, { LANGUAGES } from './i18n';

/**
 * LanguageSwitcher — MUI Select dropdown that changes the active i18next language.
 *
 * To add a language: add an entry to the LANGUAGES array in web/shared/i18n.js
 * and create web/shared/locales/<code>/{common,game,system}.json.
 *
 * Props:
 *   sx — optional MUI sx prop forwarded to the FormControl wrapper
 */
export default function LanguageSwitcher({ sx }) {
  const { t } = useTranslation();

  const handleChange = (e) => {
    i18n.changeLanguage(e.target.value);
  };

  return (
    <FormControl size="small" fullWidth sx={sx}>
      <InputLabel id="lang-select-label">{t('language.label')}</InputLabel>
      <Select
        labelId="lang-select-label"
        value={i18n.language?.split('-')[0] || 'en'}
        label={t('language.label')}
        onChange={handleChange}
      >
        {LANGUAGES.map(({ code, label }) => (
          <MenuItem key={code} value={code}>{label}</MenuItem>
        ))}
      </Select>
    </FormControl>
  );
}
